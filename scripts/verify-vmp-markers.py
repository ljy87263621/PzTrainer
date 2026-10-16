"""Audit trainer marker inventory and optional x64 input DLL (pefile + capstone)."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def verify_source():
    expected = json.loads((ROOT / "protection/markers.json").read_text(encoding="utf-8"))
    found = {}
    for path in (ROOT / "trainer/src").rglob("*.cpp"):
        text = path.read_text(encoding="utf-8-sig")
        regions = re.findall(r'PZ_VMP_BEGIN_(\w+)\("([^"]+)"\)', text)
        assert text.count("PZ_VMP_END();") == len(regions), f"Unpaired source markers: {path}"
        for mode, name in regions:
            assert name not in found, f"Duplicate marker: {name}"
            found[name] = mode
    assert found == expected, f"Marker inventory mismatch: missing={expected.keys()-found.keys()}, extra={found.keys()-expected.keys()}, modes={[(n, found[n], m) for n,m in expected.items() if n in found and found[n]!=m]}"
    print(f"Source: {len(found)} unique marker regions; one lexical End per Begin")
    return expected


def verify_pe(path, expected):
    import capstone
    from capstone.x86_const import X86_OP_MEM, X86_REG_RIP, X86_REG_RCX
    import pefile

    pe = pefile.PE(str(path))
    assert pe.FILE_HEADER.Machine == 0x8664, "Expected x64 PE"
    imports = {}
    dlls = []
    base = pe.OPTIONAL_HEADER.ImageBase
    for entry in pe.DIRECTORY_ENTRY_IMPORT:
        dll = entry.dll.decode().lower()
        dlls.append(dll)
        for symbol in entry.imports:
            if dll == "vmprotectsdk64.dll" and symbol.name:
                imports[symbol.address] = symbol.name.decode()
    required = {"VMProtectBeginMutation", "VMProtectBeginUltra", "VMProtectEnd"}
    assert required <= set(imports.values()) <= required | {"VMProtectDecryptStringA", "VMProtectDecryptStringW"}, imports
    assert not any(n.startswith(("vcruntime", "msvcp", "ucrtbase", "api-ms-win-crt")) for n in dlls), dlls

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    found = Counter()
    modes = {"VMProtectBeginMutation": "MUTATION", "VMProtectBeginUltra": "ULTRA"}
    # MSVC can split prolog/body into chained unwind entries. Group them so
    # an RCX load in the prolog is retained for the Begin call in the body.
    functions = {}
    entries = {f.struct.BeginAddress: f for f in pe.DIRECTORY_ENTRY_EXCEPTION}
    for function in pe.DIRECTORY_ENTRY_EXCEPTION:
        start, end = function.struct.BeginAddress, function.struct.EndAddress
        owner = function
        while owner.unwindinfo.UNW_FLAG_CHAININFO:
            owner = entries[owner.unwindinfo.FunctionEntry]
        functions.setdefault(owner.struct.BeginAddress, []).append((start, end))
    for start, ranges in functions.items():
        current_name = None
        begins, ends = [], 0
        instructions = (instruction for low, high in sorted(ranges)
                        for instruction in md.disasm(pe.get_data(low, high-low), base+low))
        for instruction in instructions:
            ops = instruction.operands
            if instruction.mnemonic == "lea" and len(ops) == 2 and ops[0].reg == X86_REG_RCX and ops[1].type == X86_OP_MEM and ops[1].mem.base == X86_REG_RIP:
                rva = instruction.address + instruction.size + ops[1].mem.disp - base
                value = pe.get_string_at_rva(rva)
                current_name = value.decode("ascii", errors="replace") if value else None
            if instruction.mnemonic != "call" or not ops or ops[0].type != X86_OP_MEM or ops[0].mem.base != X86_REG_RIP:
                continue
            target = instruction.address + instruction.size + ops[0].mem.disp
            api = imports.get(target)
            if api in modes:
                assert current_name in expected, f"Unrecognized marker name at {instruction.address:x}: {current_name}"
                assert expected[current_name] == modes[api], (current_name, api)
                begins.append(current_name)
                found[current_name] += 1
            elif api == "VMProtectEnd":
                ends += 1
            current_name = None
        if begins:
            assert len(begins) == 1 and ends >= 1, f"Missing/overlapping binary boundaries at {start:x}: {begins}, ends={ends}"
    assert found == Counter({name: 1 for name in expected}), f"Compiled marker references mismatch: {found}"

    helpers = []
    for kind in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if kind.id == 10:
            for resource in kind.directory.entries:
                if resource.id == 103:
                    data = resource.directory.entries[0].data.struct
                    helpers.append(pe.get_data(data.OffsetToData, data.Size))
    assert helpers == [(ROOT / "trainer/src/runtime/resources/pztrainer-player-overrides.jar").read_bytes()], "Embedded helper differs from source resource"
    print(f"PE: x64, /MT import check, {sum(found.values())} named Begin call sites with matching modes/End, exact embedded player helper")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path, nargs="?")
    args = parser.parse_args()
    inventory = verify_source()
    if args.dll:
        verify_pe(args.dll, inventory)

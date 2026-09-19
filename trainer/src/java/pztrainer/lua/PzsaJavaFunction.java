package pztrainer.lua;

import se.krka.kahlua.vm.JavaFunction;
import se.krka.kahlua.vm.LuaCallFrame;

/** JavaFunction adapter used by the native PZSA Lua registration layer. */
public final class PzsaJavaFunction implements JavaFunction {
    private final long functionId;

    public PzsaJavaFunction(long functionId) {
        this.functionId = functionId;
    }

    @Override
    public int call(LuaCallFrame frame, int nArguments) {
        return dispatch(functionId, frame, nArguments);
    }

    private static native int dispatch(long functionId, LuaCallFrame frame,
                                       int nArguments);
}

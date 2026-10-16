from pathlib import Path
source=(Path(__file__).resolve().parents[1]/'trainer/src/bridge/jni_game_bridge.cpp').read_text()
assert 'g_model_outline_bridge.ApplyToObject' not in source, 'ESP collection must not mutate game outline registries from the overlay/render thread'
assert 'g_model_outline_bridge.BeginFrame' not in source, 'Native capture must not depend on the unrelated object-outline UI clock'
print('ESP collection: no legacy outline-registry writes or UI-clock capture gate.')

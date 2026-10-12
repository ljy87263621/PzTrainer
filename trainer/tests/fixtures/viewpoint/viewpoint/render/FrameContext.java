package viewpoint.render;

import org.joml.Matrix4f;

public final class FrameContext {
    public SceneData scene;
    public Matrix4f plainViewProjection = new Matrix4f();
    public float eyeX;
    public float eyeY;
    public float eyeZ;
}

package zombie.characters;

public class IsoPlayer {
    public static IsoPlayer instance;
    public boolean aiming;
    public float delay;
    public int clears;

    public static IsoPlayer getInstance() { return instance; }
    public boolean isAiming() { return aiming; }
    public float getAimingDelay() { return delay; }
    public void setAimingDelay(float value) {
        if (!Thread.currentThread().getName().equals("MainThread")) {
            throw new IllegalStateException("Game mutation must run on MainThread");
        }
        delay = value;
        clears++;
    }
}

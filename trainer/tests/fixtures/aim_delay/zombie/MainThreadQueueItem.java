package zombie;

public class MainThreadQueueItem implements Runnable {
    private final Runnable task;
    private volatile boolean finished;
    private Throwable thrown;

    private MainThreadQueueItem(Runnable task) { this.task = task; }
    public static MainThreadQueueItem alloc(Runnable task) { return new MainThreadQueueItem(task); }
    public boolean isFinished() { return finished; }
    public Throwable getThrown() { return thrown; }
    public void run() {
        try { task.run(); } catch (Throwable error) { thrown = error; }
        finally { finished = true; }
    }
}

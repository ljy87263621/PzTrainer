package zombie;

public class MainThread {
    public static void queueInvokeOnMainThread(MainThreadQueueItem task) {
        Thread thread = new Thread(task, "MainThread");
        thread.setDaemon(true);
        thread.start();
    }
    public static void invokeOnMainThread(Runnable task) {
        Thread thread = new Thread(task, "MainThread");
        thread.start();
        try { thread.join(); } catch (InterruptedException error) { Thread.currentThread().interrupt(); }
    }
}

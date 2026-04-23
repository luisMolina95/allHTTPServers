package portable.java.examples;

import java.net.InetAddress;
import java.net.InetSocketAddress;

import java.net.StandardProtocolFamily;

import java.nio.ByteBuffer;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.ArrayList;

import java.lang.reflect.Field;
import java.io.FileDescriptor;

public class NonBlockinEcho {

    static int getFD(Object ch) throws Exception {
        Field fdField = ch.getClass().getDeclaredField("fd");
        fdField.setAccessible(true);

        Object fd = fdField.get(ch);

        Field fdInt = FileDescriptor.class.getDeclaredField("fd");
        fdInt.setAccessible(true);

        return fdInt.getInt(fd);
    }

    public static final int QUEUE_SIZE = 10;
    public static final SocketChannel[] queue = new SocketChannel[QUEUE_SIZE];
    public static int count = 0;

    private static void queueAdd(SocketChannel value) {
        if (count == QUEUE_SIZE) {
            System.out.println("Queue full!");
            System.exit(1);
        }
        queue[count] = value;
        count++;
    }

    private static void queueRemove(int index) {
        if (count == 0) {
            return;
        }

        for (int i = index; i < count; i++) {
            queue[i] = queue[i + 1];
        }
        count = count - 1;
    }

    private static void printQueue() throws Exception {
        System.out.print("Queue: [");

        if (count > 0) {
            System.out.print("%d".formatted(getFD(queue[0])));

        }
        for (int i = 1; i < count; i++) {
            System.out.print(",%d".formatted(getFD(queue[i])));
        }
        System.out.println("]");
    }

    public static void main() throws Exception {

        // Create AF_INET address (TCP)
        byte INADDR_ANY[] = { 0, 0, 0, 0 }; // 0.0.0.0
        InetAddress addr = InetAddress.getByAddress(INADDR_ANY);
        InetSocketAddress sockaddr_in = new InetSocketAddress(addr, 8080);

        // Bind Socket 0 backlog
        // ServerSocket socket = new ServerSocket(8080, 0, addr);
        ServerSocketChannel server = ServerSocketChannel.open(StandardProtocolFamily.INET);

        server.configureBlocking(false);
        server.bind(sockaddr_in, 0);
        System.out.println("Server(%d)(%d)".formatted(getFD(server), server.socket().getLocalPort()));

        ByteBuffer buffer = ByteBuffer.allocate(4096);

        // Accepting connections
        while (true) {

            SocketChannel client = server.accept();

            if (client != null) {
                client.configureBlocking(false);
                System.out.println("Client(%d)".formatted(client.socket().getPort()));
                queueAdd(client);
                printQueue();
            }

            int r = 0;
            for (int i = 0; i < count; i++) {
                SocketChannel c = queue[i];

                buffer.position(0);
                buffer.limit(4096);

                r = c.read(buffer);

                if (r == 0) {
                    continue;
                }

                if (r == -1) {
                    c.close();
                    queueRemove(i);
                    printQueue();
                    continue;
                }

                buffer.position(0);
                buffer.limit(r);

                int p = 0;
                while (p < r) {
                    int w = c.write(buffer);
                    System.out.println("written(%d)".formatted(w));
                    p += w;
                }

            }

        }

        // server.close();
    }

    public static void opaque() throws Exception {

        var server = ServerSocketChannel.open();
        server.configureBlocking(false);
        server.bind(new InetSocketAddress(8080));

        System.out.println("Listening on port " + server.socket().getLocalPort());

        var clients = new ArrayList<SocketChannel>();
        var buffer = ByteBuffer.allocate(4096);

        while (true) {

            var client = server.accept();
            if (client != null) {
                client.configureBlocking(false);
                clients.add(client);
                System.out.println("client(" + client.socket().getPort() + ")");
            }

            for (int i = 0; i < clients.size(); i++) {
                var c = clients.get(i);

                buffer.clear();
                int r = c.read(buffer);

                if (r == 0) {
                    continue;
                }

                if (r == -1) {
                    c.close();
                    clients.remove(i);
                    continue;
                }

                buffer.flip();

                while (buffer.hasRemaining()) {
                    c.write(buffer);
                }
            }
        }
        // server.close();
    }

}
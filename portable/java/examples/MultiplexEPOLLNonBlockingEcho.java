package portable.java.examples;

import java.io.FileDescriptor;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.StandardProtocolFamily;
import java.nio.ByteBuffer;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.channels.SelectionKey;

import java.lang.reflect.Field;

// import java.nio.charset.StandardCharsets;

public class MultiplexEPOLLNonBlockingEcho {

    public static final int BUFFER_SIZE = 4096;

    static int getFD(Object ch) throws Exception {
        Field fdField = ch.getClass().getDeclaredField("fd");
        fdField.setAccessible(true);

        Object fd = fdField.get(ch);

        Field fdInt = FileDescriptor.class.getDeclaredField("fd");
        fdInt.setAccessible(true);

        return fdInt.getInt(fd);
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

        Selector selector = Selector.open();

        server.register(selector, SelectionKey.OP_ACCEPT);

        ByteBuffer buffer = ByteBuffer.allocate(BUFFER_SIZE);
        int r = 0;
        while (true) {

            int selected = selector.select();

            System.out.println("Selected(%d)".formatted(selected));

            for (SelectionKey key : selector.selectedKeys()) {

                int ops = key.readyOps();
                System.out.println("ops(%d)".formatted(ops));
                if (key.isAcceptable()) {
                    SocketChannel client = ((ServerSocketChannel) key.channel()).accept();
                    client.configureBlocking(false);
                    client.register(selector, SelectionKey.OP_READ);
                    System.out.println("Client(%d)(%d)".formatted(getFD(client), client.socket().getPort()));

                }
                if (key.isReadable()) {

                    SocketChannel cr = ((SocketChannel) key.channel());
                    buffer.position(0);
                    buffer.limit(BUFFER_SIZE);
                    r = cr.read(buffer);

                    if (r == -1) {
                        cr.close();
                        key.cancel();
                        continue;
                    }
                    System.out.println("read(%d)".formatted(r));
                    cr.register(selector, SelectionKey.OP_WRITE);
                    buffer.position(0);
                    buffer.limit(r);

                    // System.out.println(StandardCharsets.UTF_8.decode(buffer).toString());
                }
                if (key.isWritable()) {
                    SocketChannel cw = ((SocketChannel) key.channel());
                    int w = cw.write(buffer);
                    if (r == -1) {
                        cw.close();
                        key.cancel();
                        continue;
                    }
                    System.out.println("written(%d)".formatted(w));
                    cw.register(selector, SelectionKey.OP_READ);
                }
            }
            selector.selectedKeys().clear();

            // System.exit(1);
        }

        // server.close();
    }

}

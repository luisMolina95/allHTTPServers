package portable.java.examples;

import java.net.InetAddress;
import java.net.InetSocketAddress;

import java.net.ServerSocket;
import java.net.Socket;

import java.io.InputStream;
import java.io.OutputStream;

public class BlockinEcho {

    public static void main() throws Exception {

        // Create AF_INET address (TCP)
        byte INADDR_ANY[] = { 0, 0, 0, 0 }; // 0.0.0.0
        InetAddress addr = InetAddress.getByAddress(INADDR_ANY);
        InetSocketAddress sockaddr_in = new InetSocketAddress(addr, 8080);

        // Bind Socket 0 backlog
        // ServerSocket socket = new ServerSocket(8080, 0, addr);
        ServerSocket server = new ServerSocket();
        server.bind(sockaddr_in, 0);
        System.out.println("Listening to port: " + server.getLocalPort());

        // Accepting connections
        while (true) {
            Socket client = server.accept();
            System.out.println("Client accepted on port: " + client.getPort());

            // FD abstraction?
            InputStream in = client.getInputStream();
            OutputStream out = client.getOutputStream();

            // Read and write
            // in.transferTo(out) does the same
            byte[] buffer = new byte[4096];
            int n = 0;
            while (true) {
                n = in.read(buffer, 0, 4096);
                // System.out.println("read(%d):".formatted(n));
                if (n == -1) {
                    break;
                }
                // System.out.println("%s".formatted(new String(buffer, 0, n)));
                out.write(buffer, 0, n);
                out.flush();
            }
            client.close();
        }
        // server.close();
    }

    public static void opaque() throws Exception {
        final var server = new ServerSocket(8080);

        System.out.println("Listening to port: " + server.getLocalPort());

        while (true) {
            var client = server.accept();
            var in = client.getInputStream();
            var out = client.getOutputStream();
            in.transferTo(out);
            client.close();
        }
        // server.close();
    }

}
import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.io.File;
import java.io.FileReader;

public class LTNServer {
    public static void main(String[] args) {
        try {
            ServerSocket serverSocket = new ServerSocket(8000);
            for (;;) {
                // 阻塞操作，收到消息时才会继续
                Socket socket = serverSocket.accept();
                System.out.println("收到一条请求:");
                // 读信息
                DataInputStream inputStream = new DataInputStream(socket.getInputStream());
                BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(inputStream));
                StringBuilder requestBuilder = new StringBuilder();
                String line = "";
                System.out.print("Begin to ");
                // 待办：在谷歌浏览器上这里的readLine有时候会卡住，可能是针对重复打开同一个URL做了优化机制 —— 我不会啊哥们
                line = bufferedReader.readLine();
                System.out.print("Read html request");
                while (line != null && !line.isEmpty()) {
                    System.out.print(".");
                    requestBuilder.append(line + '\n');
                    line = bufferedReader.readLine();
                }
                System.out.println(requestBuilder.toString());
                // 在正常请求之后，还有一个favicon.ico也就是网页图标的请求，不理他
                if (!requestBuilder.toString().contains("favicon.ico")) {
                    try {
                        // 我们知道url的格式是 GET <URL> 
                        String arg = requestBuilder.toString();
                        arg = arg.split(" ")[1];
                        System.out.println("其URL是" + arg);
                        execute_it(arg);
                    } catch (Exception e) {
                        System.out.println("有空再分析这种谷歌浏览器的报错现场吧，不过主要的错误还是上面的readLine卡住，改成readByte也没用");
                        e.printStackTrace();
                    }
                }
                // 返回信息
                BufferedWriter bufferedWriter = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), "utf-8"));
                // 按照http协议返回信息（按照deepseek建议加了close但是没有用）
                bufferedWriter.write("HTTP/1.1 200 OK\nConnection: close\n\n");
                reply_html_file(bufferedWriter, "test");
                bufferedWriter.flush();
                // 需要清空输入流中的剩余字节，避免下个请求无法被接收（按照deepseek建议加了一堆close和保护但是也没有用）
                try {
                    // 检查
                    if (inputStream.available() > 0) {
                        byte[] buffer = new byte[inputStream.available()];
                        inputStream.readFully(buffer); // 读取并丢弃剩余数据
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
                inputStream.close();
                bufferedReader.close();
                socket.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    public static void reply_html_file(BufferedWriter bufferedWriter, String filepath) {
        try {
            String file = "./console.html";
            FileReader reader = new FileReader(file);
            char[] ch = new char[5000];
            reader.read(ch);
            for (char c:ch) {
                bufferedWriter.write(c);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    public static void execute_it(String args) {
        try {
            // 指定程序路径与参数
            ProcessBuilder builder = new ProcessBuilder(
                "D:\\PP\\PJ03\\main.exe",
                args
            );

            // 可选：合并标准错误和标准输出流
            builder.redirectErrorStream(true);

            // 启动进程
            Process process = builder.start();

            // 读取输出
            BufferedReader reader = new BufferedReader(
                new InputStreamReader(process.getInputStream(), "UTF-8")
            );
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.println("输出: " + line);
            }
            
            // 读取返回值
            int exitCode = process.waitFor();
            System.out.println("退出码: " + exitCode);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
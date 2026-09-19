package pztrainer.lua;

import java.io.Reader;
import java.nio.charset.StandardCharsets;

/**
 * Presents a Java String as unsigned UTF-8 bytes to Project Zomboid's Kahlua
 * lexer. Its LexState stores each Reader value as one byte before decoding the
 * collected token bytes as UTF-8, so a normal StringReader corrupts non-ASCII
 * characters.
 */
public final class Utf8ByteReader extends Reader {
    private final byte[] bytes;
    private int index;

    public Utf8ByteReader(String source) {
        bytes = source.getBytes(StandardCharsets.UTF_8);
    }

    @Override
    public int read() {
        if (index >= bytes.length) {
            return -1;
        }
        return bytes[index++] & 0xFF;
    }

    @Override
    public int read(char[] buffer, int offset, int length) {
        if (index >= bytes.length) {
            return -1;
        }
        int count = Math.min(length, bytes.length - index);
        for (int item = 0; item < count; ++item) {
            buffer[offset + item] = (char)(bytes[index++] & 0xFF);
        }
        return count;
    }

    @Override
    public void close() {
    }
}

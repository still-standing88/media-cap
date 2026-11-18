using System.Runtime.InteropServices;
using System.Text;

namespace MediaCap;

internal static unsafe class Utf8
{
    private static readonly UTF8Encoding Strict = new(false, true);

    internal static byte[]? Encode(string? text)
    {
        if (text is null)
            return null;
        if (text.Contains('\0'))
            throw new ArgumentException("MediaCap strings cannot contain an embedded NUL.", nameof(text));

        var result = new byte[checked(Strict.GetByteCount(text) + 1)];
        Strict.GetBytes(text, result);
        return result;
    }

    internal static string Decode(byte* ptr, int capacity)
    {
        var bytes = new ReadOnlySpan<byte>(ptr, capacity);
        int nul = bytes.IndexOf((byte)0);
        return Encoding.UTF8.GetString(nul < 0 ? bytes : bytes[..nul]);
    }

    internal static string? Decode(byte* ptr) => Marshal.PtrToStringUTF8((nint)ptr);

    internal static string DecodeId(byte* ptr)
    {
        if (ptr is null)
            throw new InvalidDataException("MediaCap returned a null device ID.");
        return Strict.GetString(MemoryMarshal.CreateReadOnlySpanFromNullTerminated(ptr));
    }
}

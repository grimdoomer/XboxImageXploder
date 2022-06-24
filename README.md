# XboxImageXploder
XboxImageExploder is a command line tool for adding new executable code segments to original xbox executables. Use it to create code caves of any size where you can place new code or data for modifications to xbes. Multiple segments can be added and it works with both retail and debug executables.

## Usage
```
XboxImageXploder.exe <xbe_file> <section_name> <section_size>
```
Where:
- xbe_file: File path to the xbe file
- section_name: Name of the new code section
- section_size: Size of the new code section

\
Example usage to create a new segment of 8192 bytes called ".hacks":
```
XboxImageXploder.exe X:\Xbox\Test\test.xbe .hacks 8192
```
\
After the segment is created the name, virtual address, virtual size, file offset, and file size of the new segment will be printed:
```
Section Name:           .hacks
Virtual Address:        0x008ceda0
Virtual Size:           0x00002000
File Offset:            0x001c8000
File Size:              0x00002000
```

The virtual address and size will tell you where in memory the segment is located and how much memory is allocated for it. The file offset and size will tell you where in the xbe file the segment is located. This information can be used for writing your new code based on the virtual memory address, and writing it to the specified file offset in the xbe file.

## Adding new code

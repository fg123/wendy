/*
 * io.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides read(), readRaw(), and readFile()
 */

struct io => [read, readRaw, readFile, writeFile];
io.read => () native io_read;
io.readRaw => () native io_readRaw;
io.readFile => (fileName) native io_readFile;
io.writeFile => (fileName, content) native io_writeFile;

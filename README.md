# huffman

Compress input file using Huffman coding.

## Usage

Run `make` to build the `huffman` binary:

```sh
make
```

### Compress a file

```sh
./huffman myfile.txt
```

This generates a compressed file: `myfile.txt.huff`.

### Decompress a file

```sh
./huffman -d myfile.txt.huff
```

This restores the original file: `myfile.txt`.

## License

This project is licensed under the MIT License. See [LICENSE](./LICENSE) for more details.

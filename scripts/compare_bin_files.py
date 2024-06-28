import argparse


def compare_files(file1, file2, chunk_size=4096):
    total_bytes = 0
    differing_bytes = 0
    total_chunks = 0
    differing_chunks = 0

    with open(file1, "rb") as f1, open(file2, "rb") as f2:
        while True:
            b1 = f1.read(chunk_size)
            b2 = f2.read(chunk_size)

            if not b1 and not b2:
                break

            total_bytes += max(len(b1), len(b2))
            differing_bytes += sum(1 for x, y in zip(b1, b2) if x != y)
            differing_bytes += abs(len(b1) - len(b2))
            total_chunks += 1
            if b1 != b2:
                differing_chunks += 1

    if total_bytes == 0:
        return 0.0

    bytes_difference_percentage = (differing_bytes / total_bytes) * 100
    chunks_difference_percentage = (differing_chunks / total_chunks) * 100
    print(f"Total bytes: {total_bytes}, differing bytes: {differing_bytes}")
    print(f"Total chunks: {total_chunks}, differing chunks: {differing_chunks}")
    return bytes_difference_percentage, chunks_difference_percentage


def main():
    parser = argparse.ArgumentParser(
        description="Compare two binary files and calculate the percentage of differing bytes."
    )
    parser.add_argument("file1", help="The first binary file to compare")
    parser.add_argument("file2", help="The second binary file to compare")
    parser.add_argument(
        "-c",
        "--chunk_size",
        type=int,
        default=4096,
        help="The chunk size to use for reading the files (default: 1024 bytes)",
    )

    args = parser.parse_args()

    bytes_difference_percentage, chunks_difference_percentage = compare_files(
        args.file1, args.file2, args.chunk_size
    )
    print(
        f"The percentage of differing bytes is: {bytes_difference_percentage:.2f}% chunks: {chunks_difference_percentage:.2f}%"
    )


if __name__ == "__main__":
    main()

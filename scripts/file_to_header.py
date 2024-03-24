import argparse
import os


def valid_cpp_identifier(name):
    """Ensure the name is a valid C++ identifier."""
    return "".join([c if c.isalnum() else "_" for c in name])


def binary_file_to_header(input_file_path, output_file_path=None):
    base_name = os.path.basename(input_file_path)
    data_var_name = valid_cpp_identifier(base_name)
    size_var_name = f"{data_var_name}_size"

    # Read the binary file
    with open(input_file_path, "rb") as binary_file:
        binary_data = binary_file.read()

    # Convert binary data to a C++ character array initialization string
    array_content = ", ".join(f"0x{byte:02x}" for byte in binary_data)

    # Uppercase the names to match coding style
    data_var_name = data_var_name.upper()
    size_var_name = size_var_name.upper()

    # Prepare the content of the header file
    header_guard = f"{data_var_name.upper()}_HPP"
    header_content = f"""#ifndef {header_guard}
#define {header_guard}

// Array size
const size_t {size_var_name} = {len(binary_data)};

// Binary data represented as a character array
static const unsigned char {data_var_name}[{size_var_name}] = {{{array_content}}};

#endif // {header_guard}
"""

    # Determine the output file path
    if output_file_path is None:
        output_file_path = f"{data_var_name}.hpp"

    # Write the header file
    with open(output_file_path, "w") as header_file:
        header_file.write(header_content)

    print(f"Header file {output_file_path} has been created with {len(binary_data)} bytes.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert a binary file to a C++ header file.")
    parser.add_argument("input_file", help="Path to the input binary file.")
    parser.add_argument("-o", "--output", help="Path to the output header file. Optional.")
    args = parser.parse_args()

    binary_file_to_header(args.input_file, args.output)

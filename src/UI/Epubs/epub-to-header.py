def binary_file_to_header(input_file_path):
    # Read the binary file
    with open(input_file_path, 'rb') as binary_file:
        binary_data = binary_file.read()
    
    # Convert binary data to a C++ character array initialization string
    array_content = ', '.join(f'0x{byte:02x}' for byte in binary_data)
    
    # Prepare the content of the header file
    header_content = f"""#ifndef ONBOARDING_EPUB1_HPP
#define ONBOARDING_EPUB1_HPP

// Array size
const size_t dataSize = {len(binary_data)};

// Binary data represented as a character array
unsigned char data[dataSize] = {{{array_content}}};

#endif // ONBOARDING_EPUB1_HPP
"""
    
    # Write the header file
    with open('OnboardingEpub1.hpp', 'w') as header_file:
        header_file.write(header_content)

    print(f"Header file OnboardingEpub1.hpp has been created with {len(binary_data)} bytes.")


binary_file_to_header('./book/d7570c42-18a9-4795-a44e-9cee09b9fe3e.epub')

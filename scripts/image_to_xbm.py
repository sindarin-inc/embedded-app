import argparse
import os

from PIL import Image, ImageOps


def convert_bw_img_to_xbm(input_path, output_path):
    img = Image.open(input_path)

    # Check if image has transparency (an alpha channel)
    if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
        # Create a white background image
        background = Image.new(img.mode[:-1], img.size, (255, 255, 255))
        # Paste the image on the background, using the alpha channel as mask
        background.paste(img, img.split()[-1])
        img = background

    # Convert the image to 1 bit mode (black and white)
    img = img.convert("1")

    # Invert the image to match how AdafruitGFX draws bitmaps
    img = ImageOps.invert(img)

    # Save the image in XBM format
    img.save(output_path, "xbm")

    # Open the XBM file and read its content
    with open(output_path, "r") as f:
        content = f.read()

    # Extract the base name of the input file
    base_name = os.path.splitext(os.path.basename(input_path))[0]

    # Replace the variable names
    content = content.replace("im_width", base_name + "_width")
    content = content.replace("im_height", base_name + "_height")
    content = content.replace("im_bits", base_name + "_bits")
    content = content.replace("static char", "static const unsigned char")

    # Write the modified content back to the XBM file
    with open(output_path, "w") as f:
        f.write(content)


def main():
    parser = argparse.ArgumentParser(description="Converts a B&W image to XBM format.")
    parser.add_argument("input", help="The input image file path.")
    parser.add_argument("output", help="The output XBM file path.")
    args = parser.parse_args()

    convert_bw_img_to_xbm(args.input, args.output)


if __name__ == "__main__":
    main()

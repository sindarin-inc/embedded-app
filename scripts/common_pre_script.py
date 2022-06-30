import os

hash = os.popen('git rev-parse --short HEAD', 'r', 1).read().strip()

#create header file containing git hash
fp = open("src/Misc/GitHash.hpp", 'w')
fp.write("#ifndef GITHASH_HPP\n")
fp.write("#define GITHASH_HPP\n")
fp.write("const char* githash = \"")
fp.write(hash)
fp.write("\";\n")
fp.write("#endif\n")
fp.close()
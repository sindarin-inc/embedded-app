import os

hash = os.popen('git rev-parse --short HEAD', 'r', 1).read().strip()
dirty = os.popen('git diff --quiet || echo \'-dirty\'').read().strip()

githash_string = hash + dirty
print("-DGITHASH='\"%s\"'" % githash_string)
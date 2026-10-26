#!/bin/bash

major_version=$(i2cget -y -f 12 0x15 0x11)
major_version=${major_version:2:2}
major_version=$((16#$major_version))

echo $major_version

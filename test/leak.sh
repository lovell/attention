curl -O https://raw.githubusercontent.com/jcupitt/libvips/master/libvips.supp
curl -O https://raw.githubusercontent.com/lovell/sharp/master/test/leak/sharp.supp

cd ../
G_SLICE=always-malloc G_DEBUG=gc-friendly valgrind --suppressions=test/libvips.supp --suppressions=test/sharp.supp --leak-check=full --show-leak-kinds=definite,indirect,possible --num-callers=20 --trace-children=yes npm test

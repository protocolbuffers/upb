shopt -s globstar
sed -i -f rename.sed **/*.c **/*.cc **/*.h **/*.hpp 

# compile paq8sfx (stub + paq8sfx) on Lubuntu (x64) with gcc
# prepare with:
# sudo apt update
# sudo apt install gcc g++
#------------------------
# if you're on arch, use these instead:
# sudo pacman -Syu
# sudo pacman -S gcc g++

g++ -DNDEBUG -DSFX -Os -fno-fast-math -ffp-contract=off -flto=auto -s -m64 -march=nocona -mtune=generic -Wl,--gc-sections -fno-exceptions -fno-rtti -std=gnu++17 ../src/file/*.cpp ../src/model/*.cpp ../src/*.cpp -ostub
g++ -DNDEBUG -DFULL -O3 -fno-fast-math -ffp-contract=off -flto=auto -s -m64 -march=nocona -mtune=generic -Wl,--gc-sections -fno-exceptions -fno-rtti -std=gnu++17 ../src/file/*.cpp ../src/model/*.cpp ../src/*.cpp -opaq8sfx

rm -rf *.o

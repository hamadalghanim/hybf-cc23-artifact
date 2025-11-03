# .gdbinit — run the binary with proper arguments
set args -I src/include \
          -D src/data \
          -F src/font \
          -C src/maps \
          -H src/hyph \
          data/data.lout

tui enable
break src/z06.c:Parse
# Run the program
run
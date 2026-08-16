CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

SRC_MATH = src/math/ComplexNumber.cpp src/math/Matrix.cpp src/math/Polynomial.cpp
SRC_CONTROL = src/control/TransferFunction.cpp src/control/Stability.cpp \
              src/control/RootLocus.cpp src/control/StateSpace.cpp \
              src/control/Conversions.cpp src/control/TimeResponse.cpp
SRC_VIEW = src/view/Plot.cpp src/view/Slider.cpp src/view/RootLocusWindow.cpp src/view/TimeResponseWindow.cpp

SFML_LIBS = -lsfml-graphics -lsfml-window -lsfml-system

# Console menu -- now needs SFML too, since choosing "Root locus" opens
# the plot window directly (showRootLocusPlot, in view/RootLocusWindow.cpp).
main: src/main.cpp $(SRC_MATH) $(SRC_CONTROL) $(SRC_VIEW)
	$(CXX) $(CXXFLAGS) -o main.exe $^ $(SFML_LIBS)

# Math/control validation target: no GUI dependency at all, so it always
# builds even without SFML installed. Run after touching any math/control file.
test_math: test/test_math.cpp $(SRC_MATH) $(SRC_CONTROL)
	$(CXX) $(CXXFLAGS) -o test_math.exe $^

# Quick-launch: opens the root locus window directly for the book's own
# example, skipping the console menu. Shares showRootLocusPlot with main.
gui: src/main_gui.cpp $(SRC_MATH) $(SRC_CONTROL) $(SRC_VIEW)
	$(CXX) $(CXXFLAGS) -o gui.exe $^ $(SFML_LIBS)

clean:
	rm -f main main.exe test_math test_math.exe gui gui.exe

.PHONY: clean

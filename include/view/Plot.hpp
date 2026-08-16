#ifndef PLOT_HPP
#define PLOT_HPP

#include <vector>
#include <utility>
#include <SFML/Graphics.hpp>
#include "math/ComplexNumber.hpp"

// A plot area: a rectangle of pixels on screen that represents a rectangle
// of the complex plane (real part on the x axis, imaginary part on the y
// axis). Everything drawn goes through toScreen() first, which is the only
// place the pixel<->world conversion happens.
class Plot {
private:
    float pixelX, pixelY, pixelWidth, pixelHeight;
    double worldXMin, worldXMax, worldYMin, worldYMax;

public:
    Plot(float pixelX, float pixelY, float pixelWidth, float pixelHeight,
         double worldXMin, double worldXMax, double worldYMin, double worldYMax);

    sf::Vector2f toScreen(double worldX, double worldY) const;
    sf::Vector2f toScreen(const ComplexNumber& z) const;

    void drawFrame(sf::RenderWindow& window) const;
    void drawAxes(sf::RenderWindow& window) const;
    void drawLine(sf::RenderWindow& window, sf::Vector2f a, sf::Vector2f b, sf::Color color) const;

    void drawPolyline(sf::RenderWindow& window, const std::vector<ComplexNumber>& points, sf::Color color) const;

    // Same idea, but for plots that aren't the complex plane (e.g. a time
    // response, where x = t and y = output). Each pair is (worldX, worldY).
    void drawPolyline(sf::RenderWindow& window, const std::vector<std::pair<double, double>>& points, sf::Color color) const;

    // Pole marker: an "X", the usual root-locus convention.
    void drawPoleMarker(sf::RenderWindow& window, const ComplexNumber& pole, sf::Color color) const;
    // Zero marker: an "O".
    void drawZeroMarker(sf::RenderWindow& window, const ComplexNumber& zero, sf::Color color) const;
    // Current closed-loop pole: a filled dot, so it stands out from the
    // open-loop pole/zero markers above.
    void drawFilledMarker(sf::RenderWindow& window, const ComplexNumber& point, sf::Color color) const;
};

#endif

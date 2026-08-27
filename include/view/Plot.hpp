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
    // Inverse of toScreen(): what world point is under a given screen
    // (pixel) position. Used to pan/zoom around the mouse.
    std::pair<double, double> toWorld(sf::Vector2f screenPoint) const;

    // Whether a screen point falls inside this plot's pixel rectangle --
    // check before starting a drag or a zoom so scrolling/dragging
    // elsewhere in the window (e.g. over a slider) doesn't move the view.
    bool containsScreenPoint(sf::Vector2f screenPoint) const;

    // Shifts the view by a screen-space pixel delta (the distance the
    // mouse moved since the last frame while dragging), keeping the zoom
    // level the same -- click-and-drag panning.
    void pan(float pixelDX, float pixelDY);

    // Zooms in/out by `factor` (less than 1 zooms in, more than 1 zooms
    // out) around a fixed screen point -- typically the mouse position,
    // e.g. for scroll-wheel zoom -- so whatever's under the cursor stays
    // under the cursor rather than the view re-centering on you.
    void zoom(double factor, sf::Vector2f aroundScreenPoint);

    double getWorldXMin() const;
    double getWorldXMax() const;
    double getWorldYMin() const;
    double getWorldYMax() const;
    // Replaces the view outright -- e.g. a "Reset view" button restoring
    // the original autofit bounds after the user's panned/zoomed away.
    void setWorldBounds(double xMin, double xMax, double yMin, double yMax);

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

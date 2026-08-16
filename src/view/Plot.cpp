#include "view/Plot.hpp"
#include <cmath>

using namespace std;

Plot::Plot(float px, float py, float pw, float ph,
           double xMin, double xMax, double yMin, double yMax)
    : pixelX(px), pixelY(py), pixelWidth(pw), pixelHeight(ph),
      worldXMin(xMin), worldXMax(xMax), worldYMin(yMin), worldYMax(yMax) {}

sf::Vector2f Plot::toScreen(double worldX, double worldY) const {
    float fx = static_cast<float>((worldX - worldXMin) / (worldXMax - worldXMin));
    float fy = static_cast<float>((worldY - worldYMin) / (worldYMax - worldYMin));
    float screenX = pixelX + fx * pixelWidth;
    float screenY = pixelY + (1.0f - fy) * pixelHeight;
    return sf::Vector2f(screenX, screenY);
}

sf::Vector2f Plot::toScreen(const ComplexNumber& z) const {
    return toScreen(z.getReal(), z.getImag());
}

void Plot::drawFrame(sf::RenderWindow& window) const {
    sf::RectangleShape rect(sf::Vector2f(pixelWidth, pixelHeight));
    rect.setPosition({pixelX, pixelY});
    rect.setFillColor(sf::Color(25, 25, 30));
    rect.setOutlineColor(sf::Color(90, 90, 100));
    rect.setOutlineThickness(1.0f);
    window.draw(rect);
}

void Plot::drawLine(sf::RenderWindow& window, sf::Vector2f a, sf::Vector2f b, sf::Color color) const {
    sf::Vertex line[] = {
        sf::Vertex{a, color},
        sf::Vertex{b, color}
    };
    window.draw(line, 2, sf::PrimitiveType::Lines);
}

void Plot::drawAxes(sf::RenderWindow& window) const {
    if(worldYMin < 0 && worldYMax > 0){
        drawLine(window, toScreen(worldXMin, 0.0), toScreen(worldXMax, 0.0), sf::Color(120, 120, 130));
    }
    if(worldXMin < 0 && worldXMax > 0){
        drawLine(window, toScreen(0.0, worldYMin), toScreen(0.0, worldYMax), sf::Color(120, 120, 130));
    }
}

void Plot::drawPolyline(sf::RenderWindow& window, const vector<ComplexNumber>& points, sf::Color color) const {
    for(size_t i = 0; i + 1 < points.size(); i++){
        drawLine(window, toScreen(points[i]), toScreen(points[i + 1]), color);
    }
}

void Plot::drawPolyline(sf::RenderWindow& window, const vector<pair<double, double>>& points, sf::Color color) const {
    for(size_t i = 0; i + 1 < points.size(); i++){
        drawLine(window, toScreen(points[i].first, points[i].second),
                         toScreen(points[i + 1].first, points[i + 1].second), color);
    }
}

void Plot::drawPoleMarker(sf::RenderWindow& window, const ComplexNumber& pole, sf::Color color) const {
    sf::Vector2f center = toScreen(pole);
    float size = 5.0f;
    drawLine(window, sf::Vector2f(center.x - size, center.y - size),
                     sf::Vector2f(center.x + size, center.y + size), color);
    drawLine(window, sf::Vector2f(center.x - size, center.y + size),
                     sf::Vector2f(center.x + size, center.y - size), color);
}

void Plot::drawZeroMarker(sf::RenderWindow& window, const ComplexNumber& zero, sf::Color color) const {
    sf::CircleShape circle(6.0f);
    circle.setOrigin({6.0f, 6.0f});
    circle.setPosition(toScreen(zero));
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineColor(color);
    circle.setOutlineThickness(2.0f);
    window.draw(circle);
}

void Plot::drawFilledMarker(sf::RenderWindow& window, const ComplexNumber& point, sf::Color color) const {
    sf::CircleShape circle(5.0f);
    circle.setOrigin({5.0f, 5.0f});
    circle.setPosition(toScreen(point));
    circle.setFillColor(color);
    window.draw(circle);
}
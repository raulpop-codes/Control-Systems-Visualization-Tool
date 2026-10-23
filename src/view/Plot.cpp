#include "view/Plot.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

Plot::Plot(float px, float py, float pw, float ph,
           double xMin, double xMax, double yMin, double yMax)
    : pixelX(px), pixelY(py), pixelWidth(pw), pixelHeight(ph),
      worldXMin(xMin), worldXMax(xMax), worldYMin(yMin), worldYMax(yMax),
      dragging(false), lastMouse{}, logX(false), fixedStepX(0.0), fixedStepY(0.0) {}

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

pair<double, double> Plot::toWorld(sf::Vector2f screenPoint) const {
    double fx = (screenPoint.x - pixelX) / pixelWidth;
    double fy = 1.0 - (screenPoint.y - pixelY) / pixelHeight;
    double worldX = worldXMin + fx * (worldXMax - worldXMin);
    double worldY = worldYMin + fy * (worldYMax - worldYMin);
    return {worldX, worldY};
}

bool Plot::containsScreenPoint(sf::Vector2f screenPoint) const {
    return screenPoint.x >= pixelX && screenPoint.x <= pixelX + pixelWidth
        && screenPoint.y >= pixelY && screenPoint.y <= pixelY + pixelHeight;
}

void Plot::pan(float pixelDX, float pixelDY) {
    double spanX = worldXMax - worldXMin;
    double spanY = worldYMax - worldYMin;
    // Derived from "the world point under the old cursor position should
    // end up under the new cursor position" -- screenY grows downward
    // while worldY (the imaginary axis) grows upward, hence the opposite
    // signs below.
    double worldDX = -static_cast<double>(pixelDX) / pixelWidth * spanX;
    double worldDY = static_cast<double>(pixelDY) / pixelHeight * spanY;
    worldXMin += worldDX; worldXMax += worldDX;
    worldYMin += worldDY; worldYMax += worldDY;
}

void Plot::zoom(double factor, sf::Vector2f aroundScreenPoint) {
    auto [anchorX, anchorY] = toWorld(aroundScreenPoint);
    double spanX = (worldXMax - worldXMin) * factor;
    double spanY = (worldYMax - worldYMin) * factor;
    // Guard against zooming in/out to a degenerate or runaway span.
    if(spanX < 1e-6 || spanY < 1e-6 || spanX > 1e12 || spanY > 1e12) return;

    double fx = (anchorX - worldXMin) / (worldXMax - worldXMin);
    double fy = (anchorY - worldYMin) / (worldYMax - worldYMin);
    worldXMin = anchorX - fx * spanX;
    worldXMax = worldXMin + spanX;
    worldYMin = anchorY - fy * spanY;
    worldYMax = worldYMin + spanY;
}

double Plot::getWorldXMin() const { return worldXMin; }
double Plot::getWorldXMax() const { return worldXMax; }
double Plot::getWorldYMin() const { return worldYMin; }
double Plot::getWorldYMax() const { return worldYMax; }

void Plot::setLogX(bool on) { logX = on; }

void Plot::setFixedSteps(double stepX, double stepY) {
    fixedStepX = stepX;
    fixedStepY = stepY;
}

void Plot::setWorldBounds(double xMin, double xMax, double yMin, double yMax) {
    worldXMin = xMin; worldXMax = xMax; worldYMin = yMin; worldYMax = yMax;
}

bool Plot::handleEvent(const sf::Event& event, sf::RenderWindow& window) {
    sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    if(const auto* mousePress = event.getIf<sf::Event::MouseButtonPressed>()){
        if(mousePress->button == sf::Mouse::Button::Left && containsScreenPoint(mouse)){
            dragging = true;
            lastMouse = mouse;
        }
    } else if(const auto* mouseRelease = event.getIf<sf::Event::MouseButtonReleased>()){
        if(mouseRelease->button == sf::Mouse::Button::Left){
            dragging = false;
        }
    } else if(event.is<sf::Event::MouseMoved>()){
        if(dragging){
            pan(mouse.x - lastMouse.x, mouse.y - lastMouse.y);
            lastMouse = mouse;
            return true;
        }
    } else if(const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()){
        if(containsScreenPoint(mouse)){
            double factor = wheel->delta > 0.0f ? 0.9 : (1.0 / 0.9);
            zoom(factor, mouse);
            return true;
        }
    }
    return false;
}

// A view covering exactly this plot's pixel rectangle, with its viewport set
// to the same rectangle (as fractions of the window): the world->pixel
// numbers from toScreen() stay valid unchanged, but anything outside the
// rectangle falls outside the view and is cut off.
void Plot::beginClip(sf::RenderWindow& window) const {
    sf::Vector2f full = window.getDefaultView().getSize();
    sf::View view(sf::FloatRect({pixelX, pixelY}, {pixelWidth, pixelHeight}));
    view.setViewport(sf::FloatRect({pixelX / full.x, pixelY / full.y},
                                    {pixelWidth / full.x, pixelHeight / full.y}));
    window.setView(view);
}

void Plot::endClip(sf::RenderWindow& window) const {
    window.setView(window.getDefaultView());
}

// Picks a "round" tick spacing -- 1, 2 or 5 times a power of ten -- that
// puts roughly targetTicks ticks across a span, so the numbers on the axes
// read 0, 2, 4, 6 ... instead of 0, 1.7, 3.4 ... at any zoom level.
static double niceStep(double span, double targetTicks) {
    double raw = span / targetTicks;
    double magnitude = pow(10.0, floor(log10(raw)));
    double fraction = raw / magnitude; // in [1, 10)
    double nice = fraction < 1.5 ? 1.0 : fraction < 3.5 ? 2.0 : fraction < 7.5 ? 5.0 : 10.0;
    return nice * magnitude;
}

// Same idea as niceStep(), but when a preferred spacing is set it is kept as
// long as it doesn't crowd the axis, and doubled (45, 90, 180, 360 ...) when
// it does.
static double stepFor(double span, double targetTicks, double preferred) {
    double raw = span / targetTicks;
    if(!(preferred > 0.0)) return niceStep(span, targetTicks);
    double step = preferred;
    for(int i = 0; i < 60 && step < raw; i++) step *= 2.0;
    return step;
}

static string formatTick(double value, double step) {
    if(fabs(value) < step * 1e-6) value = 0.0; // rounding noise would print "-0"
    ostringstream out;
    out << setprecision(6) << value;
    return out.str();
}

// The tick loops below are capped at this many ticks per axis: niceStep()
// only aims for ~10, but a view panned absurdly far from the origin (where
// (first + n) * step stops changing as n grows) could otherwise loop forever.
static const int MAX_TICKS = 100;

void Plot::drawGrid(sf::RenderWindow& window) const {
    double spanX = worldXMax - worldXMin;
    double spanY = worldYMax - worldYMin;
    if(!(spanX > 0.0) || !(spanY > 0.0) || !isfinite(spanX) || !isfinite(spanY)) return;

    sf::Color gridColor(40, 40, 48);
    double stepX = stepFor(spanX, pixelWidth / 90.0, fixedStepX);
    double stepY = stepFor(spanY, pixelHeight / 60.0, fixedStepY);

    if(logX){
        // x = log10(w): a line at every power of ten, and -- while there is
        // room for them -- fainter ones at 2, 3 ... 9 times it.
        sf::Color minorColor(28, 28, 36);
        bool showMinor = spanX <= 8.0;
        int count = 0;
        for(int k = static_cast<int>(floor(worldXMin)); k <= worldXMax && count < MAX_TICKS; k++, count++){
            for(int m = 1; m <= 9; m++){
                if(m > 1 && !showMinor) break;
                double x = k + log10(static_cast<double>(m));
                if(x < worldXMin || x > worldXMax) continue;
                drawLine(window, toScreen(x, worldYMin), toScreen(x, worldYMax), m == 1 ? gridColor : minorColor);
            }
        }
    } else {
        double firstX = ceil(worldXMin / stepX);
        for(int n = 0; n < MAX_TICKS && (firstX + n) * stepX <= worldXMax; n++){
            double x = (firstX + n) * stepX;
            drawLine(window, toScreen(x, worldYMin), toScreen(x, worldYMax), gridColor);
        }
    }
    double firstY = ceil(worldYMin / stepY);
    for(int n = 0; n < MAX_TICKS && (firstY + n) * stepY <= worldYMax; n++){
        double y = (firstY + n) * stepY;
        drawLine(window, toScreen(worldXMin, y), toScreen(worldXMax, y), gridColor);
    }
}

void Plot::drawTickLabels(sf::RenderWindow& window, const sf::Font* font) const {
    double spanX = worldXMax - worldXMin;
    double spanY = worldYMax - worldYMin;
    if(!(spanX > 0.0) || !(spanY > 0.0) || !isfinite(spanX) || !isfinite(spanY)) return;

    sf::Color tickColor(150, 150, 160);
    double stepX = stepFor(spanX, pixelWidth / 90.0, fixedStepX);
    double stepY = stepFor(spanY, pixelHeight / 60.0, fixedStepY);
    float bottom = pixelY + pixelHeight;

    // The x ticks first as (world x, text) pairs, so the drawing below is the
    // same whether the axis is linear or logarithmic.
    vector<pair<double, string>> xTicks;
    if(logX){
        // One label per power of ten -- every 2nd, 5th ... one when the view
        // spans so many decades that the numbers would overlap.
        int every = max(1, static_cast<int>(ceil(spanX / (pixelWidth / 70.0))));
        int count = 0;
        for(int k = static_cast<int>(ceil(worldXMin)); k <= worldXMax && count < MAX_TICKS; k++, count++){
            if(k % every != 0) continue;
            xTicks.push_back({static_cast<double>(k), formatTick(pow(10.0, k), 0.0)});
        }
    } else {
        double firstX = ceil(worldXMin / stepX);
        for(int n = 0; n < MAX_TICKS && (firstX + n) * stepX <= worldXMax; n++){
            double x = (firstX + n) * stepX;
            xTicks.push_back({x, formatTick(x, stepX)});
        }
    }
    for(auto& tick : xTicks){
        float screenX = toScreen(tick.first, worldYMin).x;
        drawLine(window, sf::Vector2f(screenX, bottom), sf::Vector2f(screenX, bottom + 4.0f), tickColor);
        if(font != nullptr){
            sf::Text text(*font, tick.second, 12);
            sf::FloatRect b = text.getLocalBounds();
            text.setOrigin({b.position.x + b.size.x / 2.0f, 0.0f}); // centered under the tick
            text.setPosition({screenX, bottom + 6.0f});
            text.setFillColor(tickColor);
            window.draw(text);
        }
    }

    double firstY = ceil(worldYMin / stepY);
    for(int n = 0; n < MAX_TICKS && (firstY + n) * stepY <= worldYMax; n++){
        double y = (firstY + n) * stepY;
        float screenY = toScreen(worldXMin, y).y;
        drawLine(window, sf::Vector2f(pixelX - 4.0f, screenY), sf::Vector2f(pixelX, screenY), tickColor);
        if(font != nullptr){
            sf::Text text(*font, formatTick(y, stepY), 12);
            sf::FloatRect b = text.getLocalBounds();
            text.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y / 2.0f}); // right-aligned, centered on the tick
            text.setPosition({pixelX - 8.0f, screenY});
            text.setFillColor(tickColor);
            window.draw(text);
        }
    }
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
    if(!logX && worldXMin < 0 && worldXMax > 0){
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
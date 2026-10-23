#include "view/TimeResponseWindow.hpp"
#include "view/Plot.hpp"
#include "view/Button.hpp"
#include "view/Font.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace std;

void showTimeResponsePlot(const vector<ResponseSample>& samples, const string& windowTitle) {
    if(samples.empty()){
        cerr << "showTimeResponsePlot: no samples to draw.\n";
        return;
    }

    sf::RenderWindow window(sf::VideoMode({900, 550}), windowTitle);
    window.setFramerateLimit(60);
    sf::Font font = loadFont();

    // An unstable system's output grows without bound and can overflow to
    // inf/NaN late in a long simulation; those samples can't be placed on
    // any axis, so they're left out of both the fit and the curve.
    vector<pair<double, double>> curve;
    curve.reserve(samples.size());
    for(auto& s : samples){
        if(isfinite(s.t) && isfinite(s.y)) curve.emplace_back(s.t, s.y);
    }
    if(curve.empty()){
        cerr << "showTimeResponsePlot: every sample is infinite or NaN.\n";
        return;
    }

    // Autofit: the view starts out covering the whole simulation in t and
    // the full range the output reaches in y.
    double tMin = curve.front().first;
    double tMax = curve.back().first;
    if(tMax - tMin < 1e-12) tMax = tMin + 1.0; // guard against a single sample

    double yMin = curve[0].second, yMax = curve[0].second;
    for(auto& p : curve){
        yMin = min(yMin, p.second);
        yMax = max(yMax, p.second);
    }
    // Pad the y range so the curve isn't drawn flush against the frame, and
    // so a perfectly flat response (yMin == yMax, e.g. an all-zero output)
    // still gets a sensible span instead of dividing by zero in Plot.
    double ySpan = yMax - yMin;
    if(ySpan < 1e-9) ySpan = (fabs(yMax) > 1e-9) ? fabs(yMax) : 1.0;
    double yPad = ySpan * 0.1;
    yMin -= yPad;
    yMax += yPad;

    Plot plot(70.0f, 50.0f, 780.0f, 400.0f, tMin, tMax, yMin, yMax);
    Button resetViewButton(760.0f, 15.0f, 90.0f, 28.0f, "Reset view");

    while(window.isOpen()){
        while(const auto event = window.pollEvent()){
            if(event->is<sf::Event::Closed>()) window.close();

            if(resetViewButton.handleEvent(*event, window)){
                plot.setWorldBounds(tMin, tMax, yMin, yMax);
            }

            // Drag to pan, scroll to zoom -- the tick labels follow.
            plot.handleEvent(*event, window);
        }

        window.clear(sf::Color(15, 15, 20));

        plot.drawFrame(window);
        plot.beginClip(window);
        plot.drawGrid(window);
        plot.drawAxes(window);
        plot.drawPolyline(window, curve, sf::Color(90, 190, 240));
        plot.endClip(window);
        plot.drawTickLabels(window, &font);

        sf::Text title(font, windowTitle, 20);
        title.setPosition(sf::Vector2f{70.0f, 15.0f});
        title.setFillColor(sf::Color(230, 230, 235));
        window.draw(title);

        resetViewButton.draw(window, &font);

        sf::Text hint(font, "Drag to pan, scroll to zoom", 12);
        hint.setPosition(sf::Vector2f{575.0f, 22.0f});
        hint.setFillColor(sf::Color(140, 140, 150));
        window.draw(hint);

        // Axis names: time along the bottom, output up the side.
        sf::Text tLabel(font, "t (s)", 13);
        tLabel.setPosition(sf::Vector2f{805.0f, 478.0f});
        tLabel.setFillColor(sf::Color(170, 200, 240));
        window.draw(tLabel);

        sf::Text yLabel(font, "y(t)", 13);
        yLabel.setPosition(sf::Vector2f{14.0f, 30.0f});
        yLabel.setFillColor(sf::Color(170, 200, 240));
        window.draw(yLabel);

        window.display();
    }
}

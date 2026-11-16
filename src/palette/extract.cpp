#include "palette/extract.h"
#include "palette/oklab.h"

#include <QImage>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <numeric>
#include <random>
#include <ranges>

namespace {

constexpr int sampleSide = 128;
constexpr int clusterCount = 16;
constexpr int maxRounds = 50;
// Any constant does: it only has to make k-means++ pick the same seeds every run.
constexpr std::mt19937::result_type randomSeed = 1;

// A face is clamped so that the ramp's darkest tone stays above the floor and its lightest below
// the ceiling, and so that a whole panel of it is not loud.
constexpr double toneFloor = 0.2;
constexpr double toneCeiling = 0.97;
constexpr double faceChroma = 0.05;
// k-means spends several clusters on one large region; clusters closer than this are one color.
constexpr double sameColor = 0.04;
// Colors whose faces differ by less than this in hue and chroma and in lightness give one
// candidate, which they must together cover this much of the image to make.
constexpr double faceHueApart = 0.03;
constexpr double faceLightnessApart = 0.1;
constexpr double faceShare = 0.04;
constexpr std::size_t candidateCount = 6;
// Less than this of the image is noise and names nothing.
constexpr double namedShare = 0.01;
// How far a named color's lightness is kept from the face's, so that it reads against it.
constexpr double selectionApart = 0.25;
constexpr double fillApart = 0.15;
constexpr double deskApart = 0.15;
constexpr double inkLightness = 0.2;
constexpr double inkChroma = 0.03;
constexpr double neutralChroma = 0.04;
constexpr double hueFamily = 30 * std::numbers::pi / 180;

double chroma(const Oklab& c) { return std::hypot(c.a, c.b); }

double distance2(const Oklab& p, const Oklab& q)
{
    const double l = p.lightness - q.lightness;
    const double a = p.a - q.a;
    const double b = p.b - q.b;
    return l * l + a * a + b * b;
}

double distance(const Oklab& p, const Oklab& q) { return std::sqrt(distance2(p, q)); }

Oklab limitChroma(Oklab c, double limit)
{
    if (const double now = chroma(c); now > limit) {
        c.a *= limit / now;
        c.b *= limit / now;
    }
    return c;
}

// `c` with its lightness at least `apart` from the face's: darker unless it is lighter already
// and there is room above.
Oklab apartFrom(Oklab c, const Oklab& face, double apart)
{
    if (std::abs(c.lightness - face.lightness) >= apart)
        return c;
    const bool above = c.lightness > face.lightness && face.lightness + apart <= toneCeiling;
    c.lightness = above ? face.lightness + apart : std::max(face.lightness - apart, 0.0);
    return c;
}

Oklab darkerThan(Oklab c, const Oklab& face, double apart)
{
    c.lightness = std::min(c.lightness, face.lightness - apart);
    return c;
}

// Pixels picked rather than averaged, so that small details keep their colors instead of
// blending into their surroundings.
std::vector<Oklab> samples(const QImage& image)
{
    QImage small = image.width() > sampleSide || image.height() > sampleSide
        ? image.scaled(sampleSide, sampleSide, Qt::KeepAspectRatio, Qt::FastTransformation)
        : image;
    small.convertTo(QImage::Format_ARGB32);
    std::vector<Oklab> pixels;
    pixels.reserve(std::size_t(small.width()) * small.height());
    for (int y = 0; y < small.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x)
            if (qAlpha(line[x]) >= 128)
                pixels.push_back(toOklab(QColor::fromRgb(line[x])));
    }
    return pixels;
}

// k-means++: each seed after the first is a pixel picked with odds that grow with its squared
// distance from the seeds so far. Stops early when every pixel sits on a seed.
std::vector<Oklab> seeds(const std::vector<Oklab>& pixels)
{
    std::mt19937 random(randomSeed);
    // mt19937's output is fixed by the standard; the distributions over it are not.
    const auto uniform = [&random] { return random() / 4294967296.0; };
    std::vector<Oklab> chosen { pixels[random() % pixels.size()] };
    std::vector<double> closest(pixels.size(), std::numeric_limits<double>::max());
    std::vector<double> cumulative(pixels.size());
    while (chosen.size() < clusterCount) {
        for (std::size_t i = 0; i < pixels.size(); ++i)
            closest[i] = std::min(closest[i], distance2(pixels[i], chosen.back()));
        std::partial_sum(closest.begin(), closest.end(), cumulative.begin());
        if (cumulative.back() == 0)
            break;
        const double target = uniform() * cumulative.back();
        chosen.push_back(pixels[std::ranges::upper_bound(cumulative, target) - cumulative.begin()]);
    }
    return chosen;
}

struct Cluster {
    Oklab centre;
    int count = 0;
};

int nearest(const std::vector<Cluster>& clusters, const Oklab& pixel)
{
    const auto closest = std::ranges::min_element(clusters, { },
        [&pixel](const Cluster& cluster) { return distance2(cluster.centre, pixel); });
    return int(closest - clusters.begin());
}

// Each cluster at the mean of its pixels. A cluster left empty keeps its centre.
void moveCentres(
    std::vector<Cluster>& clusters, const std::vector<Oklab>& pixels, const std::vector<int>& of)
{
    std::vector<Oklab> sums(clusters.size());
    for (Cluster& cluster : clusters)
        cluster.count = 0;
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        Oklab& sum = sums[of[i]];
        sum.lightness += pixels[i].lightness;
        sum.a += pixels[i].a;
        sum.b += pixels[i].b;
        ++clusters[of[i]].count;
    }
    for (std::size_t k = 0; k < clusters.size(); ++k)
        if (const int n = clusters[k].count)
            clusters[k].centre = { sums[k].lightness / n, sums[k].a / n, sums[k].b / n };
}

// Largest first, each cluster absorbs the smaller ones that are the same color and keeps its
// centre.
std::vector<Cluster> merged(std::vector<Cluster> clusters)
{
    std::ranges::stable_sort(clusters, std::greater { }, &Cluster::count);
    std::vector<Cluster> kept;
    for (const Cluster& cluster : clusters) {
        const auto same = std::ranges::find_if(kept, [&cluster](const Cluster& other) {
            return distance(other.centre, cluster.centre) < sameColor;
        });
        if (same == kept.end())
            kept.push_back(cluster);
        else
            same->count += cluster.count;
    }
    std::ranges::stable_sort(kept, std::greater { }, &Cluster::count);
    return kept;
}

Oklab clampFace(Oklab c)
{
    c.lightness = std::clamp(c.lightness, toneFloor - lightnessOffset(darkestTone),
        toneCeiling - lightnessOffset(lightestTone));
    return limitChroma(c, faceChroma);
}

bool alike(const Oklab& face, const Oklab& other)
{
    return std::hypot(face.a - other.a, face.b - other.b) < faceHueApart
        && std::abs(face.lightness - other.lightness) < faceLightnessApart;
}

// The colors whose faces come out alike: the most common of them, its face, and their share.
struct FaceGroup {
    std::size_t source = 0;
    Oklab face;
    double share = 0;
};

std::vector<FaceGroup> faceGroups(std::span<const ImageColor> colors)
{
    std::vector<FaceGroup> groups;
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const Oklab face = clampFace(toOklab(colors[i].color));
        auto group = std::ranges::find_if(
            groups, [&face](const FaceGroup& group) { return alike(group.face, face); });
        if (group == groups.end())
            group = groups.insert(groups.end(), { i, face });
        group->share += colors[i].share;
    }
    std::ranges::stable_sort(groups, std::greater { }, &FaceGroup::share);
    return groups;
}

struct Source {
    std::size_t index = 0;
    Oklab lab;
    double share = 0;
    bool taken = false;
};

bool sameHue(const Oklab& p, const Oklab& q)
{
    const double cp = chroma(p);
    const double cq = chroma(q);
    if (cp < neutralChroma || cq < neutralChroma)
        return cp < neutralChroma && cq < neutralChroma;
    return (p.a * q.a + p.b * q.b) / (cp * cq) > std::cos(hueFamily);
}

struct EarlierPicks {
    std::vector<Oklab> selection;
    std::vector<Oklab> fill;
    std::vector<Oklab> desk;
};

// Dividing by the colors of its hue taken before spreads the accents over hues and candidates.
Oklab best(std::vector<Source>& sources, std::vector<Oklab>& mine, std::vector<Oklab>& earlier,
    auto score, bool colorful)
{
    const bool anyLeft = std::ranges::any_of(sources, std::not_fn(&Source::taken));
    auto pool = sources
        | std::views::filter([anyLeft](const Source& source) { return !anyLeft || !source.taken; });
    const auto weighed = [&](const Source& source) {
        const auto alike = [&source](const Oklab& before) { return sameHue(before, source.lab); };
        const auto count
            = std::ranges::count_if(mine, alike) + std::ranges::count_if(earlier, alike);
        return std::pair(colorful && chroma(source.lab) >= neutralChroma,
            std::invoke(score, source) / double(1 + count));
    };
    Source& chosen = *std::ranges::max_element(pool, { }, weighed);
    chosen.taken = true;
    mine.push_back(chosen.lab);
    earlier.push_back(chosen.lab);
    return chosen.lab;
}

// Selection and fill are the colors most unlike the face in hue and chroma, the pager desk the
// most common of the rest, and ink the darkest, darkened until text in it reads.
void nameColors(
    Palette& palette, const Oklab& face, std::vector<Source>& others, EarlierPicks& earlier)
{
    if (std::ranges::all_of(others, &Source::taken))
        return;
    const auto colorfulness = [&face](const Source& source) {
        const Oklab& c = source.lab;
        return std::hypot(c.a - face.a, c.b - face.b) + chroma(c);
    };
    std::vector<Oklab> mine;
    palette.selection = fromOklab(darkerThan(
        best(others, mine, earlier.selection, colorfulness, true), face, selectionApart));
    palette.fill = fromOklab(
        apartFrom(best(others, mine, earlier.fill, colorfulness, true), face, fillApart));
    palette.pagerDesk = fromOklab(
        apartFrom(best(others, mine, earlier.desk, &Source::share, false), face, deskApart));
    const auto darkest = std::ranges::min_element(
        others, { }, [](const Source& source) { return source.lab.lightness; });
    Oklab ink = limitChroma(darkest->lab, inkChroma);
    ink.lightness = std::min(ink.lightness, inkLightness);
    palette.ink = fromOklab(ink);
}

} // namespace

std::vector<ImageColor> imageColors(const QImage& image)
{
    const std::vector<Oklab> pixels = samples(image);
    if (pixels.empty())
        return { };
    std::vector<Cluster> clusters;
    for (const Oklab& centre : seeds(pixels))
        clusters.push_back({ centre });
    std::vector<int> of(pixels.size(), -1);
    for (int round = 0; round < maxRounds; ++round) {
        bool changed = false;
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            const int k = nearest(clusters, pixels[i]);
            changed = changed || k != of[i];
            of[i] = k;
        }
        moveCentres(clusters, pixels, of);
        if (!changed)
            break;
    }

    std::vector<ImageColor> colors;
    for (const Cluster& cluster : merged(std::move(clusters)))
        if (cluster.count)
            colors.push_back(
                { fromOklab(cluster.centre), double(cluster.count) / double(pixels.size()) });
    return colors;
}

std::vector<Palette> candidates(std::span<const ImageColor> colors)
{
    std::vector<Source> sources;
    for (std::size_t i = 0; i < colors.size(); ++i)
        if (colors[i].share >= namedShare)
            sources.push_back({ i, toOklab(colors[i].color), colors[i].share });
    EarlierPicks earlier;
    std::vector<Palette> palettes;
    for (const FaceGroup& group : faceGroups(colors)) {
        if (group.share < faceShare || palettes.size() == candidateCount)
            break;
        for (Source& source : sources)
            source.taken = source.index == group.source;
        Palette palette;
        palette.face = fromOklab(group.face);
        nameColors(palette, group.face, sources, earlier);
        palettes.push_back(palette);
    }
    return palettes;
}

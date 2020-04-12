// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "Curve.h"
#include "Opcode.h"
#include "SIMDHelpers.h"
#include "Debug.h"
#include "spline/spline.h"
#include <cmath>

namespace sfz
{

static const Curve defaultCurve = Curve::buildBipolar(0.0, 1.0);

Curve Curve::buildCurveFromHeader(
    absl::Span<const Opcode> members, Interpolator itp, bool limit)
{
    Curve curve;
    bool fillStatus[NumValues] = {};
    const Range<float> fullRange { -HUGE_VALF, +HUGE_VALF };

    auto setPoint = [&curve, &fillStatus](int i, float x) {
        curve._points[i] = x;
        fillStatus[i] = true;
    };

    // fill curve ends with default values (verified)
    setPoint(0, 0.0);
    setPoint(NumValues - 1, 1.0);

    for (const Opcode& opc : members) {
        if (opc.lettersOnlyHash != hash("v&"))
            continue;

        unsigned index = opc.parameters.back();
        if (index >= NumValues)
            continue;

        auto valueOpt = readOpcode<float>(opc.value, fullRange);
        if (!valueOpt)
            continue;

        setPoint(static_cast<int>(index), *valueOpt);
    }

    curve.fill(itp, fillStatus);

    if (limit) {
        for (unsigned i = 0; i < NumValues; ++i)
            curve._points[i] = clamp(curve._points[i], -1.0f, +1.0f);
    }

    return curve;
}

Curve Curve::buildPredefinedCurve(int index)
{
    Curve curve;

    switch (index) {
    default:
        ASSERTFALSE;
        // fallthrough
    case 0:
        curve = buildBipolar(0, 1);
        break;
    case 1:
        curve = buildBipolar(-1, +1);
        break;
    case 2:
        curve = buildBipolar(1, 0);
        break;
    case 3:
        curve = buildBipolar(+1, -1);
        break;
    case 4:
        for (unsigned i = 0; i < NumValues; ++i) {
            double x = i * (1.0 / (NumValues - 1));
            curve._points[i] = x * x;
        }
        break;
    case 5:
        for (unsigned i = 0; i < NumValues; ++i) {
            double x = i * (1.0 / (NumValues - 1));
            curve._points[i] = std::pow(x, 0.5);
        }
        break;
    case 6:
        for (unsigned i = 0; i < NumValues; ++i) {
            double x = i * (1.0 / (NumValues - 1));
            curve._points[i] = std::pow(1.0 - x, 0.5);
        }
        break;
    }

    return curve;
}

Curve Curve::buildBipolar(float v1, float v2)
{
    Curve curve;
    bool fillStatus[NumValues] = {};

    curve._points[0] = v1;
    curve._points[NumValues - 1] = v2;

    fillStatus[0] = true;
    fillStatus[NumValues - 1] = true;

    curve.lerpFill(fillStatus);
    return curve;
}

const Curve& Curve::getDefault()
{
    return defaultCurve;
}

void Curve::fill(Interpolator itp, const bool fillStatus[NumValues])
{
    switch (itp) {
    default:
    case Interpolator::Linear:
        lerpFill(fillStatus);
        break;
    case Interpolator::Spline:
        splineFill(fillStatus);
        break;
    }
}

void Curve::lerpFill(const bool fillStatus[NumValues])
{
    int left { 0 };
    int right { 1 };
    auto pointSpan = absl::MakeSpan(_points);

    while (right < NumValues) {
        for (; right < NumValues && !fillStatus[right]; ++right);
        const auto length = right - left;
        if (length > 1) {
            const float mu = (_points[right] - _points[left]) / length;
            linearRamp<float>(pointSpan.subspan(left, length), _points[left], mu);
        }
        left = right++;
    }
}

void Curve::splineFill(const bool fillStatus[NumValues])
{
    std::array<double, NumValues> x;
    std::array<double, NumValues> y;
    int count = 0;

    for (unsigned i = 0; i < NumValues; ++i) {
        if (fillStatus[i]) {
            x[count] = i;
            y[count] = _points[i];
            ++count;
        }
    }

    if (count < 3)
        return lerpFill(fillStatus);

    Spline spline(x.data(), y.data(), count);
    for (unsigned i = 0; i < NumValues; ++i) {
        if (!fillStatus[i])
            _points[i] = spline.interpolate(i);
    }
}

///
CurveSet CurveSet::createPredefined()
{
    CurveSet cs;
    cs._curves.reserve(16);

    for (int i = 0; i < Curve::NumPredefinedCurves; ++i) {
        Curve curve = Curve::buildPredefinedCurve(i);
        cs._curves.emplace_back(new Curve(curve));
    }

    return cs;
}

void CurveSet::addCurve(const Curve& curve, int explicitIndex)
{
    std::unique_ptr<Curve>* slot;

    if (explicitIndex < -1)
        return;

    if (explicitIndex >= config::maxCurves)
        return;

    if (explicitIndex == -1) {
        if (_useExplicitIndexing)
            return; // reject implicit indices if any were explicit before
        _curves.emplace_back();
        slot = &_curves.back();
    } else {
        size_t index = static_cast<unsigned>(explicitIndex);
        if (index >= _curves.size())
            _curves.resize(index + 1);
        _useExplicitIndexing = true;
        slot = &_curves[index];
    }

    slot->reset(new Curve(curve));
}

void CurveSet::addCurveFromHeader(absl::Span<const Opcode> members)
{
    auto findOpcode = [members](uint64_t name_hash) -> const Opcode* {
        const Opcode* opc = nullptr;
        for (size_t i = members.size(); !opc && i-- > 0;) {
            if (members[i].lettersOnlyHash == name_hash)
                opc = &members[i];
        }
        return opc;
    };

    int curveIndex = -1;
    Curve::Interpolator itp = Curve::Interpolator::Linear;

    if (const Opcode* opc = findOpcode(hash("curve_index"))) {
        if (auto opt = readOpcode<int>(opc->value, {0, 255}))
            curveIndex = *opt;
        else
            DBG("Invalid value for curve index: " << opc->value);
    }

#if 0 // potential sfizz extension
    if (const Opcode* opc = findOpcode(hash("sfizz:curve_interpolator"))) {
        if (opc->value == "spline")
            itp = Curve::Interpolator::Spline;
        else if (opc->value != "linear")
            DBG("Invalid value for curve interpolator: " << opc.value);
    }
#endif

    addCurve(Curve::buildCurveFromHeader(members, itp), curveIndex);
}

const Curve& CurveSet::getCurve(unsigned index) const
{
    const Curve* curve = nullptr;

    if (index < _curves.size())
        curve = _curves[index].get();

    if (!curve)
        curve = &Curve::getDefault();

    return *curve;
}

} // namespace sfz

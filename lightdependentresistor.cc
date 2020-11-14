#include "lightdependentresistor.hh"
#include "math.h"

LightDependentResistor::LightDependentResistor(uint32_t other_resistor, ePhotoCellKind kind, bool photocellOnGround) :
_other_resistor (other_resistor),
_photocell_on_ground (photocellOnGround)

{
  switch (kind) {
    case ePhotoCellKind::GL5516:
      _mult_value = 29634400;
      _pow_value = 1.6689;
    break;
    case ePhotoCellKind::GL5537_1:
      _mult_value = 32435800;
      _pow_value = 1.4899;
    break;
    case ePhotoCellKind::GL5537_2:
      _mult_value = 2801820;
      _pow_value = 1.1772;
    break;
    case ePhotoCellKind::GL5539:
      _mult_value = 208510000;
      _pow_value = 1.4850;
    break;
    case ePhotoCellKind::GL5549:
      _mult_value = 44682100;
      _pow_value = 1.2750;
    break;
    case ePhotoCellKind::GL5528:
    default:
      _mult_value = 32017200;
      _pow_value = 1.5832;
    }
}



float LightDependentResistor::luxToFootCandles(float intensity_in_lux)
{
  return intensity_in_lux/10.764;
}

float LightDependentResistor::footCandlesToLux(float intensity_in_footcandles)
{
  return 10.764*intensity_in_footcandles;
}


float LightDependentResistor::getCurrentLux(uint16_t adcRawValue) const
{

  unsigned long photocell_resistor;

  float ratio = ((float)1024/(float)adcRawValue) - 1;
  if (_photocell_on_ground) {
    photocell_resistor = _other_resistor / ratio;
  } else {
    photocell_resistor = _other_resistor * ratio;
  }

  return _mult_value / (float)pow(photocell_resistor, _pow_value);
}

float LightDependentResistor::getCurrentFootCandles(uint16_t adcRawValue) const
{
  return luxToFootCandles(getCurrentLux(adcRawValue));
}

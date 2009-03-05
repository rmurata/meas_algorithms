/*!
 * Represent a PSF as a circularly symmetrical double Gaussian
 *
 * \file
 *
 * \ingroup algorithms
 */
#include <cmath>
#include "lsst/pex/exceptions.h"
#include "lsst/afw/image/ImageUtils.h"
#include "dgPSF.h"
#include "PSFImpl.h"

namespace lsst { namespace meas { namespace algorithms {

/************************************************************************************************************/
/**
 * Constructor for a dgPSF
 */
dgPSF::dgPSF(int width,                         ///< Number of columns in realisations of PSF
             int height,                        ///< Number of rows in realisations of PSF
             double sigma1,                     ///< Width of inner Gaussian
             double sigma2,                     ///< Width of outer Gaussian
             double b                   ///< Central amplitude of outer Gaussian (inner amplitude == 1)
            ) :
    PSF(width, height),
    _sigma1(sigma1), _sigma2(sigma2), _b(b)
{
    static bool first = true;
    if (first) {
        dgPSF::registerType("DGPSF", DGPSF);
        first = false;
    }

    if (b == 0.0 && sigma2 == 0.0) {
        _sigma2 = 1.0;                  // avoid 0/0 at centre of PSF
    }

    if (_sigma1 == 0 || _sigma2 == 0) {
        throw LSST_EXCEPT(lsst::pex::exceptions::DomainErrorException,
                          (boost::format("sigma may not be 0: %g, %g") % _sigma1 % _sigma2).str());
    }
    
    if (width > 0) {
        lsst::afw::math::DoubleGaussianFunction2<double> dg(_sigma1, _sigma2, _b);
        setKernel(lsst::afw::math::Kernel::PtrT(new lsst::afw::math::AnalyticKernel(width, height, dg)));
    }
}

/// \brief Evaluate the PSF at (dx, dy) (relative to the centre), taking the central amplitude to be 1.0
double dgPSF::doGetValue(double const dx,            ///< Desired column (relative to centre)
                         double const dy             ///< Desired row
                        ) const {
    double const r2 = dx*dx + dy*dy;
    double const psf1 = exp(-r2/(2*_sigma1*_sigma1));
    if (_b == 0) {
        return psf1;
    }
    
    double const psf2 = exp(-r2/(2*_sigma2*_sigma2));

    return (psf1 + _b*psf2)/(1 + _b);
}

/*
 * Return an Image of the the PSF at the point (x, y), setting the PSF's peak value to 1.0
 *
 * The specified position is a floating point number, and the resulting image will
 * have a PSF with the correct fractional position, with the centre within pixel (width/2, height/2)
 * Specifically, fractional positions in [0, 0.5] will appear above/to the right of the center,
 * and fractional positions in (0.5, 1] will appear below/to the left (0.9999 is almost back at middle)
 *
 * @note If a fractional position is specified, the central pixel value may not be 1.0
 */
lsst::afw::image::Image<PSF::PixelT>::Ptr dgPSF::getImage(double const x, ///< column position in parent %image
                                                          double const y  ///< row position in parent %image
                                                         ) const {
    typedef lsst::afw::image::Image<PSF::PixelT> ImageT;
    ImageT::Ptr image(new ImageT(getWidth(), getHeight()));

    double const dx = lsst::afw::image::positionToIndex(x, true).second; // fractional part of position
    double const dy = lsst::afw::image::positionToIndex(y, true).second;

    int const xcen = static_cast<int>(getWidth()/2);
    int const ycen = static_cast<int>(getHeight()/2);

    for (int iy = 0; iy != image->getHeight(); ++iy) {
        ImageT::x_iterator row = image->row_begin(iy);
        for (int ix = 0; ix != image->getWidth(); ++ix) {
            row[ix] = getValue(ix - dx - xcen, iy - dy - ycen);
        }
    }
    
    return image;                                                    
}

//
// We need to make an instance here so as to register it with Centroider
//
// \cond
namespace {                                                 \
    PSF* foo = new dgPSF(0, 0, 1.0);
}

// \endcond
}}}

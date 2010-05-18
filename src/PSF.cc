// -*- LSST-C++ -*-
/*!
 * \brief Implementation of PSF code
 *
 * \file
 *
 * \ingroup algorithms
 */
#include <typeinfo>
#include <cmath>
#include "lsst/afw/image/ImagePca.h"
#include "lsst/afw/math/SpatialCell.h"
#include "lsst/meas/algorithms/PSF.h"
#include "lsst/meas/algorithms/SpatialModelPsf.h"

/************************************************************************************************************/

namespace afwImage = lsst::afw::image;
namespace afwMath = lsst::afw::math;

namespace lsst {
namespace meas {
namespace algorithms {

PSF::PSF(int const width,               // desired width of Image realisations of the kernel
         int const height               // desired height of Image realisations of the kernel; default: width
        ) :  lsst::daf::data::LsstBase(typeid(this)),
             _kernel(afwMath::Kernel::Ptr()),
             _width(width), _height(height == 0 ? width : height) {}

PSF::PSF(lsst::afw::math::Kernel::Ptr kernel ///< The Kernel corresponding to this PSF
        ) : lsst::daf::data::LsstBase(typeid(this)),
            _kernel(kernel),
            _width(kernel.get()  == NULL ? 0 : kernel->getWidth()),
            _height(kernel.get() == NULL ? 0 : kernel->getHeight()) {}

/// PSF's destructor; declared pure virtual, but we still need an implementation
PSF::~PSF() {}

///
/// Set the PSF's kernel
///
void PSF::setKernel(lsst::afw::math::Kernel::Ptr kernel) {
    _kernel = kernel;
}

///
/// Return the PSF's kernel
///
afwMath::Kernel::Ptr PSF::getKernel() {
    return _kernel;
}

///
/// Return the PSF's kernel
///
boost::shared_ptr<const afwMath::Kernel> PSF::getKernel() const {
    return boost::shared_ptr<const afwMath::Kernel>(_kernel);
}

/**
 * Return an Image of the the PSF at the point (x, y), setting the sum of all the PSF's pixels to 1.0
 *
 * The specified position is a floating point number, and the resulting image will
 * have a PSF with the correct fractional position, with the centre within pixel (width/2, height/2)
 * Specifically, fractional positions in [0, 0.5] will appear above/to the right of the center,
 * and fractional positions in (0.5, 1] will appear below/to the left (0.9999 is almost back at middle)
 *
 * @note If a fractional position is specified, the central pixel value may not be 1.0
 *
 * @note This is a virtual function; we expect that derived classes will do something
 * more useful than returning a NULL pointer
 */
afwImage::Image<PSF::Pixel>::Ptr PSF::getImage(double const, ///< column position in parent %image
                                                double const  ///< row position in parent %image
                                               ) const {
    return afwImage::Image<PSF::Pixel>::Ptr();
}

/************************************************************************************************************/
/*
 * Register a factory object by name;  if the factory's NULL, return the named factory
 */
PsfFactoryBase& PSF::_registry(std::string const& name, PsfFactoryBase* factory) {
    static std::map<std::string const, PsfFactoryBase *> psfRegistry;

    std::map<std::string const, PsfFactoryBase *>::iterator el = psfRegistry.find(name);

    if (el == psfRegistry.end()) {      // failed to find name
        if (factory) {
            psfRegistry[name] = factory;
        } else {
            throw LSST_EXCEPT(lsst::pex::exceptions::NotFoundException,
                              "Unable to lookup Psf variety \"" + name + "\"");
        }
    } else {
        if (!factory) {
            factory = (*el).second;
        } else {
            throw LSST_EXCEPT(lsst::pex::exceptions::InvalidParameterException,
                              "Psf variety \"" + name + "\" is already declared");
        }
    }

    return *factory;
}

/**
 * Declare a PsfFactory for a variety "name"
 *
 * @throws lsst::pex::exceptions::InvalidParameterException if name is already declared
 */
void PSF::declare(std::string name,          ///< name of variety
                  PsfFactoryBase* factory ///< Factory to make this sort of PSF
                 ) {
    (void)_registry(name, factory);
}

/**
 * Return the named PsfFactory
 *
 * @throws lsst::pex::exceptions::NotFoundException if name can't be found
 */
PsfFactoryBase& PSF::lookup(std::string name ///< desired variety
                                 ) {
    return _registry(name, NULL);
}

/************************************************************************************************************/
/**
 * Return a Psf of the requested variety
 *
 * @throws std::runtime_error if name can't be found
 */
PSF::Ptr createPSF(std::string const& name,       ///< desired variety
                   int width,                     ///< Number of columns in realisations of PSF
                   int height,                    ///< Number of rows in realisations of PSF
                   double p0,                     ///< PSF's 1st parameter
                   double p1,                     ///< PSF's 2nd parameter
                   double p2                      ///< PSF's 3rd parameter
            ) {
    return PSF::lookup(name).create(width, height, p0, p1, p2);
}

/**
 * Return a Psf of the requested variety
 *
 * @throws std::runtime_error if name can't be found
 */
PSF::Ptr createPSF(std::string const& name,             ///< desired variety
                   lsst::afw::math::Kernel::Ptr kernel ///< Kernel specifying the PSF
                  ) {
    return PSF::lookup(name).create(kernel);
}


    

/**
 * @brief Constructor for PsfAttributes
 *
 */
PsfAttributes::PsfAttributes(
                             PSF::Ptr psf,   ///< The psf whose attributes we want
                             double const iX,  ///< the x position in the frame we want the attributes at
                             double const iY   ///< the y position in the frame we want the attributes at
                            ) {
    _psfImage = psf->getImage(iX, iY);
}

    
/**
 * @brief Compute the 'sigma' value for an equivalent gaussian psf.
 *
 */
double PsfAttributes::computeGaussianWidth() {
    
    double sum = 0.0;
    double norm = 0.0;
    double const xCen = _psfImage->getWidth()/2;
    double const yCen = _psfImage->getHeight()/2;
    for (int iY = 0; iY != _psfImage->getHeight(); ++iY) {
        int iX = 0;
        afwImage::Image<double>::x_iterator end = _psfImage->row_end(iY);
        for (afwImage::Image<double>::x_iterator ptr = _psfImage->row_begin(iY); ptr != end; ++ptr, ++iX) {
            double const x = iX - xCen;
            double const y = iY - yCen;
            double const r = std::sqrt( x*x + y*y );
            double const m = (*ptr)*r;
            norm += (*ptr)*(*ptr);
            sum += m*m;
        }
    }
    return sqrt(sum/norm);
}



/**
 * @brief Compute the first radial moment of the psf  (sum(I*r) / sum(I) )
 *
 * \note For a Gaussian N(0, alpha^2),  <r> = sqrt(pi/2) alpha
 */
double PsfAttributes::computeFirstMoment() {
    
    double sum = 0.0;
    double norm = 0.0;
    double const xCen = _psfImage->getWidth()/2;
    double const yCen = _psfImage->getHeight()/2;
    for (int iY = 0; iY != _psfImage->getHeight(); ++iY) {
        int iX = 0;
        for (afwImage::Image<double>::x_iterator ptr = _psfImage->row_begin(iY),
                                                 end = _psfImage->row_end(iY); ptr != end; ++ptr, ++iX) {
            double const x = iX - xCen;
            double const y = iY - yCen;
            double const r = std::sqrt( x*x + y*y );
            double const m = (*ptr)*r;
            norm += *ptr;
            sum += m;
        }
    }
    
    std::string errmsg("");
    if (sum < 0.0) {
        errmsg = "sum(I*r) is negative.  ";
    }
    if (norm <= 0.0) {
        errmsg += "sum(I) is <= 0.";
    }
    if (errmsg != "") {
        throw LSST_EXCEPT(lsst::pex::exceptions::DomainErrorException, errmsg);
    }
    
    return sum/norm;
}


/**
 * @brief Compute the second radial moment of the psf  ( sum(I*r^2) / sum(I) )
 *
 * \note For a Gaussian N(0, alpha^2),  <r^2> = 2 alpha^2
 */
double PsfAttributes::computeSecondMoment() {
    
    double sum = 0.0;
    double norm = 0.0;
    double const xCen = _psfImage->getWidth()/2;
    double const yCen = _psfImage->getHeight()/2;
    for (int iY = 0; iY != _psfImage->getHeight(); ++iY) {
        int iX = 0;
        for (afwImage::Image<double>::x_iterator ptr = _psfImage->row_begin(iY),
                                                 end = _psfImage->row_end(iY); ptr != end; ++ptr, ++iX) {
            double const x = iX - xCen;
            double const y = iY - yCen;
            double const r2 = x*x + y*y;
            double const m = (*ptr)*r2;
            norm += *ptr;
            sum += m;
        }
    }
    
    std::string errmsg("");
    if (sum < 0.0) {
        errmsg = "sum(I*r*r) is negative.  ";
    }
    if (norm <= 0.0) {
        errmsg += "sum(I) is <= 0.";
    }
    if (errmsg != "") {
        throw LSST_EXCEPT(lsst::pex::exceptions::DomainErrorException, errmsg);
    }

    return sum/norm;
}


    
    
/**
 * @brief Compute the effective area of the psf ( (sum(I))^2 / sum(I^2) )
 *
 */
double PsfAttributes::computeEffectiveArea() {
    
    double sum = 0.0;
    double sumsqr = 0.0;
    for (int iY = 0; iY != _psfImage->getHeight(); ++iY) {
        afwImage::Image<double>::x_iterator end = _psfImage->row_end(iY);
        for (afwImage::Image<double>::x_iterator ptr = _psfImage->row_begin(iY); ptr != end; ++ptr) {
            sum += *ptr;
            sumsqr += (*ptr)*(*ptr);
        }
    }
    return sum*sum/sumsqr;
}

    
    
}}}


//template lsst::meas::algorithms::PsfAttributes::PsfAttributes<lsst::meas::algorithms::details::dgPSF>(lsst::meas::algorithms::PSF::Ptr psf, double const, double const);


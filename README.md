# ImageMagick wand module for node and backendjs

# Requirements

Installing dependencies on CentOS:

    yum -y install libpng-devel libjpeg-turbo-devel

Installing dependencies on Mac OS X using macports:

    port install libpng jpeg


# Usage

 - `resizeImage(source, options, callback)` - resize image using ImageMagick
   - source can be a Buffer or file name
   - options can have the following properties:
     - width - output image width, if negative and the original image width is smaller than the specified, nothing happens
     - height - output image height, if negative and the original image height is smaller this the specified, nothing happens
     - quality - 0 - 99
     - out - output file name
     - ext - image extention
     - outfile - a filename where to save scaled image, if not given the binary image data is passed to the callback
     - frame - which frame to resize for animated GIFs, 0 is default, -1 to convert all frames
     - no_animation - if set to 1 it will convert GIF animation, otherwise animated GIF is simply returned as is
     - posterize - levels, 2,3,4
     - dither - 1 to dither
     - normalize - 1 to normalize image
     - quantize - number of colors
     - treedepth - 0 or 1 for quantize
     - flip - 1 to flip image
     - flop - 1 1 to flop image
     - blur_radius - radius in pixels
     - blur_sigma - std deviation in pixels
     - sharpen_radius - radius in pixels
     - sharpen_sigma - std deviation in pixels
     - brightness - -100 - 100
     - contrast - -100 - 100
     - rotate - degrees
     - opacity - 0 - 1.0
     - crop_width - crop coordinates
     - crop_height
     - crop_x
     - crop_y
     - bgcolor - RGB for background
     - filter - box, triangle, hermite, hanning, hamming, blackman, gaussian,
                quadratic, cubic, catrom, mitchell, lanczos, kaiser, welsh, parzen,
                bohman, barlett, lagrange, jinc, sinc, sincfast, lanczossharp, lanzos2,
                lanzos2sharp, robidoux, robidouxsharp, cosine, spline, lanczosradius
     - colorspace - rgb, gray, transparent, ohta, xyz, ycbcr, ycc, yiq, ypbpr, yuv,
                    cmylk, srgb, hls, hwb
     - gravity - northwest, north, northeast, west, center, east, southwest, south, southeast

  On return the callback will receive the image data if no outfile was specified or null, and the third
  argument if an object with the result image information: file, ext, height, width, orientation, rotation. 

  The original image dimentions are returned as _width and _height.

  The file where the image is sved will have the actual extention, if the outfile parameter contains invalid extention
  it will be replaced with the actual resulting image type.

```javascript
  require("bkjs-wand").resizeImage("a.png", { width: 120, height: 120 }, function(err, data, info) {
     if (!err) fs.writeFile("b.png", data);
     console.log(err, info);
  })
```

# Author

Vlad Seryakov


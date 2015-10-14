# ImageMagick wand module for node and backendjs

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


 - `resizeImageSync(name,width,height,format,filter,quality,outfile)` - resize an image synchronically

```javascript
  var wand = require("bkjs-wand");

  wand.resizeImage("a.png", { width: 120, height: 120 }, function(err, data) {
     if (!err) fs.writeFile("b.png", data);
  })
```

# Author 

Vlad Seryakov


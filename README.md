# ImageMagick wand module for node and backendjs

# Usage

 - `resizeImage(source, options, callback)` - resize image using ImageMagick
   - source can be a Buffer or file name
   - options can have the following properties:
     - width - output image width, if negative and the original image width is smaller than the specified, nothing happens
     - height - output image height, if negative and the original image height is smaller this the specified, nothing happens
     - quality - 0 -99
     - out - output file name
     - ext - image extention
 - `resizeImageSync(name,width,height,format,filter,quality,outfile)` - resize an image synchronically

# Author 

Vlad Seryakov


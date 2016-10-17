//
//  Author: Vlad Seryakov vseryakov@gmail.com
//  April 2013
//

#include "bkjs.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <algorithm>
#include <vector>
#include <string>

#ifdef USE_WAND
#include <MagickWand/MagickWand.h>

// Async request for magickwand resize callback
class MagickBaton {
public:
    MagickBaton(): cb(0), image(0), exception(0), err(0) {
        memset(&d, 0, sizeof(d));
        filter = LanczosFilter;
    }
    ~MagickBaton() {
        if (cb) delete cb;
    }
    Nan::Callback *cb;
    unsigned char *image;
    char *exception;
    string format;
    string path;
    string out;
    FilterType filter;
    int err;
    size_t length;
    string bgcolor;
    struct {
        int quality;
        int width;
        int height;
        double blur_radius;
        double blur_sigma;
        double sharpen_radius;
        double sharpen_sigma;
        double brightness;
        double contrast;
        int crop_x;
        int crop_y;
        int crop_width;
        int crop_height;
        int posterize;
        int quantize;
        int tree_depth;
        bool normalize;
        bool flip;
        bool flop;
        DitherMethod dither;
        double rotate;
        double opacity;
    } d;
};

static FilterType getMagickFilter(string filter)
{
    return filter == "point" ? PointFilter :
                  filter == "box" ? BoxFilter :
                  filter == "triangle" ? TriangleFilter :
                  filter == "hermite" ? HermiteFilter :
                  filter == "hanning" ? HanningFilter :
                  filter == "hamming" ? HammingFilter :
                  filter == "blackman" ? BlackmanFilter :
                  filter == "gaussian" ?  GaussianFilter :
                  filter == "quadratic" ? QuadraticFilter :
                  filter == "cubic" ? CubicFilter :
                  filter == "catrom" ? CatromFilter :
                  filter == "mitchell" ? MitchellFilter :
                  filter == "lanczos" ? LanczosFilter :
                  filter == "kaiser" ? KaiserFilter:
                  filter == "welsh" ? WelshFilter :
                  filter == "parzen" ? ParzenFilter :
                  filter == "bohman" ? BohmanFilter:
                  filter == "barlett" ? BartlettFilter:
                  filter == "lagrange" ? LagrangeFilter:
#ifdef JincFilter
                  filter == "jinc" ? JincFilter :
                  filter == "sinc" ? SincFilter :
                  filter == "sincfast" ? SincFastFilter :
                  filter == "lanczossharp" ? LanczosSharpFilter:
                  filter == "lanzos2" ? Lanczos2Filter:
                  filter == "lanzos2sharp" ? Lanczos2SharpFilter:
                  filter == "robidoux" ? RobidouxFilter:
                  filter == "robidouxsharp" ? RobidouxSharpFilter:
                  filter == "cosine" ? CosineFilter:
                  filter == "spline" ? SplineFilter:
                  filter == "lanczosradius" ? LanczosRadiusFilter:
#endif
                  LanczosFilter;
}

static vector<string> bkStrSplit(const string str, const string delim, const string quotes)
{
    vector<string> rc;
    string::size_type i = 0, j = 0, q = string::npos;
    const string::size_type len = str.length();

    while (i < len) {
        i = str.find_first_not_of(delim, i);
        if (i == string::npos) break;
        // Opening quote
        if (i && quotes.find(str[i]) != string::npos) {
            q = ++i;
            while (q < len) {
                q = str.find_first_of(quotes, q);
                // Ignore escaped quotes
                if (q == string::npos || str[q - 1] != '\\') break;
                q++;
            }
        }
        // End of the word
        j = str.find_first_of(delim, i);
        if (j == string::npos) {
            rc.push_back(str.substr(i, q != string::npos ? q - i : str.size() - i));
            break;
        } else {
            rc.push_back(str.substr(i, q != string::npos ? q - i : j - i));
        }
        i = q != string::npos ? q + 1 : j + 1;
        q = string::npos;
    }
    return rc;
}

static bool bkMakePath(string path)
{
    string dir;
    vector<string> list = bkStrSplit(path, "/", "");
    for (uint i = 0; i < list.size() - 1; i++) {
        dir += list[i] + '/';
        if (mkdir(dir.c_str(), 0755)) {
            if (errno != EEXIST) {
                fprintf(stderr, "%s: %s", dir.c_str(), strerror(errno));
                return 0;
            }
        }
    }
    return 1;
}

static void doResizeImage(uv_work_t *req)
{
    MagickBaton *baton = (MagickBaton *)req->data;
    MagickWand *wand = NewMagickWand();
    MagickBooleanType status;
    ExceptionType severity;

    if (baton->image) {
        status = MagickReadImageBlob(wand, baton->image, baton->length);
        free(baton->image);
        baton->image = NULL;
    } else {
        status = MagickReadImage(wand, baton->path.c_str());
    }
    int width = MagickGetImageWidth(wand);
    int height = MagickGetImageHeight(wand);
    if (status == MagickFalse) goto err;

    // Negative width or height means we should not upscale if the image is already below the given dimensions
    if (baton->d.width < 0) {
        baton->d.width *= -1;
        if (width <= baton->d.width) baton->d.width = 0;
    }
    if (baton->d.height < 0) {
        baton->d.height *= -1;
        if (height <= baton->d.height) baton->d.height = 0;
    }

    // Keep the aspect if no dimensions given
    if (baton->d.height == 0 || baton->d.width == 0) {
        float aspectRatio = (width * 1.0)/height;
        if (baton->d.height == 0) baton->d.height = baton->d.width * (1.0/aspectRatio); else
        if (baton->d.width == 0) baton->d.width = baton->d.height * aspectRatio;
    }
    if (baton->d.crop_width && baton->d.crop_height) {
        status = MagickCropImage(wand, baton->d.crop_width, baton->d.crop_height, baton->d.crop_x, baton->d.crop_y);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.rotate) {
        PixelWand *bg = NewPixelWand();
        PixelSetColor(bg, baton->bgcolor.c_str());
        status = MagickRotateImage(wand, bg, baton->d.rotate);
        DestroyPixelWand(bg);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.opacity) {
        status = MagickSetImageAlpha(wand, baton->d.opacity);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.normalize) {
        status = MagickNormalizeImage(wand);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.posterize) {
        status = MagickPosterizeImage(wand, baton->d.posterize, baton->d.dither);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.quantize) {
        status = MagickQuantizeImage(wand, baton->d.quantize, RGBColorspace, baton->d.tree_depth, baton->d.dither, (MagickBooleanType)0);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.flip) {
        status = MagickFlipImage(wand);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.flop) {
        status = MagickFlopImage(wand);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.width && baton->d.height) {
        status = MagickResizeImage(wand, baton->d.width, baton->d.height, baton->filter);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.blur_radius || baton->d.blur_sigma) {
        status = MagickAdaptiveBlurImage(wand, baton->d.blur_radius, baton->d.blur_sigma);
        if (status == MagickFalse) goto err;
    }
    if (baton->d.brightness || baton->d.contrast) {
#ifdef JincFilter
        status = MagickBrightnessContrastImage(wand, baton->d.brightness, baton->d.contrast);
        if (status == MagickFalse) goto err;
#endif
    }
    if (baton->d.sharpen_radius || baton->d.sharpen_sigma) {
        status = MagickAdaptiveSharpenImage(wand, baton->d.sharpen_radius, baton->d.sharpen_sigma);
        if (status == MagickFalse) goto err;
    }
    if (baton->format.size()) {
        const char *fmt = baton->format.c_str();
        while (fmt && *fmt && *fmt == '.') fmt++;
        MagickSetImageFormat(wand, fmt);
    }
    if (baton->d.quality <= 100) {
        MagickSetImageCompressionQuality(wand, baton->d.quality);
    }
    if (baton->out.size()) {
        // Make sure all subdirs exist
        if (bkMakePath(baton->out)) {
            status = MagickWriteImage(wand, baton->out.c_str());
            if (status == MagickFalse) goto err;
        } else {
            baton->err = errno;
        }
    } else {
        baton->image = MagickGetImageBlob(wand, &baton->length);
        if (!baton->image) goto err;
    }
    DestroyMagickWand(wand);
    return;
err:
    baton->exception = MagickGetException(wand, &severity);
    DestroyMagickWand(wand);
}

static void afterResizeImage(uv_work_t *req)
{
    Nan::HandleScope scope;
    MagickBaton *baton = (MagickBaton *)req->data;

    Local<Value> argv[4];

    if (baton->cb && !baton->cb->IsEmpty()) {
        if (baton->err || baton->exception) {
            argv[0] = Exception::Error(String::New(baton->err ? strerror(baton->err) : baton->exception));
            NAN_TRY_CATCH_CALL(Nan::GetCurrentContext()->Global(), baton->cb, 1, argv);
        } else
        if (baton->image) {
            Buffer *buf = Buffer::New((const char*)baton->image, baton->length);
            argv[0] = Nan::New(Nan::Null());
            argv[1] = Local<Value>::New(buf->handle_);
            argv[2] = Nan::New(baton->d.width);
            argv[3] = Nan::New(baton->d.height);
            NAN_TRY_CATCH_CALL(Nan::GetCurrentContext()->Global(), baton->cb, 4, argv);
        } else {
            argv[0] = Nan::New(Nan::Null());
            NAN_TRY_CATCH_CALL(Nan::GetCurrentContext()->Global(), baton->cb, 1, argv);
        }
    }
    if (baton->image) MagickRelinquishMemory(baton->image);
    if (baton->exception) MagickRelinquishMemory(baton->exception);
    delete baton;
    delete req;
}

static NAN_METHOD(resizeImage)
{
    NAN_REQUIRE_ARGUMENT(0);
    NAN_REQUIRE_ARGUMENT_OBJECT(1, opts);
    NAN_EXPECT_ARGUMENT_FUNCTION(-1, cb);

    uv_work_t *req = new uv_work_t;
    MagickBaton *baton = new MagickBaton;
    req->data = baton;
    if (!cb.IsEmpty()) baton->cb = new Nan::Callback(cb);

    const Local<Array> names = opts->GetPropertyNames();
    for (uint i = 0 ; i < names->Length(); ++i) {
        String::Utf8Value key(names->Get(i));
        String::Utf8Value val(opts->Get(names->Get(i))->ToString());
        if (!strcmp(*key, "posterize")) baton->d.posterize = atoi(*val); else
        if (!strcmp(*key, "dither")) baton->d.dither = (DitherMethod)atoi(*val); else
        if (!strcmp(*key, "normalize")) baton->d.normalize = (DitherMethod)atoi(*val); else
        if (!strcmp(*key, "quantize")) baton->d.quantize = atoi(*val); else
        if (!strcmp(*key, "treedepth")) baton->d.tree_depth = atoi(*val); else
        if (!strcmp(*key, "flip")) baton->d.normalize = atoi(*val); else
        if (!strcmp(*key, "flop")) baton->d.flop = atoi(*val); else
        if (!strcmp(*key, "width")) baton->d.width = atoi(*val); else
        if (!strcmp(*key, "height")) baton->d.height = atoi(*val); else
        if (!strcmp(*key, "quality")) baton->d.quality = atoi(*val); else
        if (!strcmp(*key, "blur_radius")) baton->d.blur_radius = atof(*val); else
        if (!strcmp(*key, "blur_sigma")) baton->d.blur_sigma = atof(*val); else
        if (!strcmp(*key, "sharpen_radius")) baton->d.sharpen_radius = atof(*val); else
        if (!strcmp(*key, "sharpen_sigma")) baton->d.sharpen_sigma = atof(*val); else
        if (!strcmp(*key, "brightness")) baton->d.brightness = atof(*val); else
        if (!strcmp(*key, "contrast")) baton->d.contrast = atof(*val); else
        if (!strcmp(*key, "rotate")) baton->d.rotate = atof(*val); else
        if (!strcmp(*key, "opacity")) baton->d.rotate = atof(*val); else
        if (!strcmp(*key, "crop_width")) baton->d.crop_width = atoi(*val); else
        if (!strcmp(*key, "crop_height")) baton->d.crop_height = atoi(*val); else
        if (!strcmp(*key, "crop_x")) baton->d.crop_x = atoi(*val); else
        if (!strcmp(*key, "crop_y")) baton->d.crop_y = atoi(*val); else
        if (!strcmp(*key, "bgcolor")) baton->bgcolor = *val; else
        if (!strcmp(*key, "outfile")) baton->out = *val; else
        if (!strcmp(*key, "ext")) baton->format = *val; else
        if (!strcmp(*key, "filter")) baton->filter = getMagickFilter(*val);
    }

    // If a Buffer passed we use it as a source for image
    if (info[0]->IsObject()) {
        Local<Object> buf = info[0]->ToObject();
        baton->length = Buffer::Length(buf);
        baton->image = (unsigned char*)malloc(baton->length);
        memcpy(baton->image, Buffer::Data(buf), baton->length);
    } else {
        // Otherwise read form file
        Nan::Utf8String name(info[0]);
        baton->path = *name;
    }

    uv_queue_work(uv_default_loop(), req, doResizeImage, (uv_after_work_cb)afterResizeImage);
}

static NAN_METHOD(resizeImageSync)
{
    NAN_REQUIRE_ARGUMENT_AS_STRING(0, name);
    NAN_REQUIRE_ARGUMENT_INT(1, w);
    NAN_REQUIRE_ARGUMENT_INT(2, h);
    NAN_REQUIRE_ARGUMENT_AS_STRING(3, format);
    NAN_REQUIRE_ARGUMENT_AS_STRING(4, filter);
    NAN_REQUIRE_ARGUMENT_INT(5, quality);
    NAN_REQUIRE_ARGUMENT_AS_STRING(6, out);

    MagickWand *wand = NewMagickWand();
    MagickBooleanType status = MagickReadImage(wand, *name);
    if (status == MagickFalse) goto err;
    if (h <= 0 || w <= 0) {
        int width = MagickGetImageWidth(wand);
        int height = MagickGetImageHeight(wand);
        float aspectRatio = (width * 1.0)/height;
        if (h <= 0) h =  w * (1.0/aspectRatio); else
        if (w <= 0) w = h * aspectRatio;
    }
    if (w > 0 && h > 0) {
        status = MagickResizeImage(wand, w, h, getMagickFilter(*filter));
        if (status == MagickFalse) goto err;
    }
    if (format.length()) MagickSetImageFormat(wand, *format);
    if (quality <= 100) MagickSetImageCompressionQuality(wand, quality);
    status = MagickWriteImage(wand, *out);
    if (status == MagickFalse) goto err;
    DestroyMagickWand(wand);
    return;

err:
    ExceptionType severity;
    const char *str = MagickGetException(wand, &severity);
    string msg(str);
    MagickRelinquishMemory((char*)str);
    DestroyMagickWand(wand);
    ThrowException(Exception::Error(String::New(msg.c_str())));
}

void WandInit(Handle<Object> target)
{
    Nan::HandleScope scope;

    MagickWandGenesis();
    NAN_EXPORT(target, resizeImage);
    NAN_EXPORT(target, resizeImageSync);
}
#else
void WandInit(Handle<Object> target)
{
}
#endif

NODE_MODULE(binding, WandInit);

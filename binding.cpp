//
//  Author: Vlad Seryakov vseryakov@gmail.com
//  April 2013
//

#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>
#include <node_version.h>
#include <v8.h>
#include <v8-profiler.h>
#include <uv.h>
#include <nan.h>
#include <errno.h>

using namespace node;
using namespace v8;
using namespace std;

#ifdef USE_WAND

#define NAN_REQUIRE_ARGUMENT(i) if (info.Length() <= i || info[i]->IsUndefined()) {Nan::ThrowError("Argument " #i " is required");return;}
#define NAN_REQUIRE_ARGUMENT_OBJECT(i, var) if (info.Length() <= (i) || !info[i]->IsObject()) {Nan::ThrowError("Argument " #i " must be an object"); return;} Local<Object> var(info[i]->ToObject());
#define NAN_TRY_CATCH_CALL(context, callback, argc, argv) { Nan::TryCatch try_catch; (callback)->Call((context), (argc), (argv)); if (try_catch.HasCaught()) FatalException(try_catch); }

#include "MagickWand/MagickWand.h"

// Async request for magickwand resize callback
class MagickBaton {
public:
    MagickBaton(): image(0), exception(0), err(0) {
        memset(&d, 0, sizeof(d));
        filter = LanczosFilter;
        d.gravity = UndefinedGravity;
        d.colorspace = UndefinedColorspace;
    }
    ~MagickBaton() {
        cb.Reset();
    }
    Nan::Persistent<Function> cb;
    unsigned char *image;
    char *exception;
    string format;
    string path;
    string ext;
    string out;
    FilterType filter;
    int err;
    size_t length;
    string bgcolor;
    struct {
        int width;
        int height;
        int orientation;
        string ext;
    } o;
    struct {
        int quality;
        int width;
        int height;
        int frame;
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
        bool strip;
        ColorspaceType colorspace;
        DitherMethod dither;
        GravityType gravity;
        double rotate;
        double opacity;
        int orientation;
        int no_animation;
    } d;
};

static string getMagickOrientation(int type)
{
    return type == TopLeftOrientation ? "TopLeft" :
      type == TopRightOrientation ? "TopRight" :
      type == BottomRightOrientation ? "BottomRight" :
      type == BottomLeftOrientation ? "BottomLeft" :
      type == LeftTopOrientation ? "LeftTop" :
      type == RightTopOrientation ? "RightTop" :
      type == RightBottomOrientation ? "RightBottom" :
      type == LeftBottomOrientation ? "LeftBottom" : "";
}

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

static ColorspaceType getMagickColorspace(string type)
{
    return type == "rgb" ? RGBColorspace :
            type == "gray" ? GRAYColorspace:
            type == "transparent" ? TransparentColorspace :
            type == "ohta" ? OHTAColorspace :
            type == "xyz" ? XYZColorspace :
            type == "ycbcr" ? YCbCrColorspace :
            type == "ycc" ? YCCColorspace :
            type == "yiq" ? YIQColorspace :
            type == "ypbpr" ? YPbPrColorspace :
            type == "yuv" ? YUVColorspace :
            type == "cmylk" ? CMYKColorspace :
            type == "srgb" ? sRGBColorspace :
            type == "hls" ? HSLColorspace:
            type == "hwb" ? HWBColorspace : UndefinedColorspace;
}

static GravityType getMagickGravity(string type)
{
    return type == "northwest" ? NorthWestGravity :
           type == "north" ? NorthGravity :
           type == "northeast" ? NorthEastGravity :
           type == "west" ? WestGravity :
           type == "center" ? CenterGravity :
           type == "east" ? EastGravity :
           type == "southwest" ? SouthWestGravity :
           type == "south" ? SouthGravity :
           type == "southeast" ? SouthEastGravity : UndefinedGravity;
}

static int getMagickAngle(int orientation)
{
    switch (orientation) {
    case 8: return 270;
    case 6: return 90;
    case 3: return 180;
    default: return 0;
    }
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
    char *str;

    if (baton->image) {
        status = MagickReadImageBlob(wand, baton->image, baton->length);
        free(baton->image);
        baton->image = NULL;
    } else {
        status = MagickReadImage(wand, baton->path.c_str());
    }
    int frames = MagickGetNumberImages(wand);

    if (status == MagickFalse) goto err;

    // Original image properties
    baton->o.width = MagickGetImageWidth(wand);
    baton->o.height = MagickGetImageHeight(wand);
    str = MagickGetImageProperty(wand, "exif:Orientation");
    if (str) {
        baton->d.orientation = baton->o.orientation = atoi(str);
        free(str);
    }
    str = MagickGetImageFormat(wand);
    if (str) {
        baton->o.ext = str;
        free(str);
    }
    std::transform(baton->o.ext.begin(), baton->o.ext.end(), baton->o.ext.begin(), ::tolower);
    if (baton->o.ext == "jpeg") baton->o.ext = "jpg";

    // Skip animated GIF modifications until implemented properly
    if (frames <= 1 || baton->d.no_animation) {
        // Use the only the specified frame, -1 for converting all TODO
        if (frames > 1 && baton->d.frame >= 0) {
            MagickWand *lwand = MagickCoalesceImages(wand);
            if (!lwand) goto err;
            DestroyMagickWand(wand);
            wand = lwand;
            status = MagickSetIteratorIndex(wand, baton->d.frame);
            if (status == MagickFalse) goto err;
            frames = 1;
        }

        if (baton->d.rotate) {
            PixelWand *bg = NewPixelWand();
            PixelSetColor(bg, baton->bgcolor.c_str());
            status = MagickRotateImage(wand, bg, baton->d.rotate);
            DestroyPixelWand(bg);
            if (status == MagickFalse) goto err;
            // Have to strip because EXIF data is always preserved
            baton->d.orientation = 0;
            baton->d.strip = 1;
        } else
        if (baton->bgcolor.size()) {
            PixelWand *bg = NewPixelWand();
            PixelSetColor(bg, baton->bgcolor.c_str());
            status = MagickSetImageBackgroundColor(wand, bg);
            DestroyPixelWand(bg);
            if (status == MagickFalse) goto err;
            MagickWand *nwand = MagickMergeImageLayers(wand, FlattenLayer);
            if (nwand) {
                DestroyMagickWand(wand);
                wand = nwand;
            }
        }
        int width = MagickGetImageWidth(wand);
        int height = MagickGetImageHeight(wand);

        if (baton->d.crop_width && baton->d.crop_height) {
            status = MagickCropImage(wand, baton->d.crop_width, baton->d.crop_height, baton->d.crop_x, baton->d.crop_y);
            if (status == MagickFalse) goto err;
        }
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

        if (baton->d.colorspace != UndefinedColorspace) {
            status = MagickSetImageColorspace(wand, baton->d.colorspace);
            if (status == MagickFalse) goto err;
        }
        if (baton->d.gravity != UndefinedGravity) {
            status = MagickSetImageGravity(wand, baton->d.gravity);
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
        if (baton->d.strip) {
            status = MagickStripImage(wand);
            if (status == MagickFalse) goto err;
        }
        const char *fmt = baton->format.c_str();
        while (fmt && *fmt && *fmt == '.') fmt++;
        if (fmt && *fmt) {
            MagickSetImageFormat(wand, fmt);
        }
        if (baton->d.quality <= 100) {
            MagickSetImageCompressionQuality(wand, baton->d.quality);
        }
    }
    // Output info about the new or unmodified image
    baton->d.width = MagickGetImageWidth(wand);
    baton->d.height = MagickGetImageHeight(wand);
    str = MagickGetImageFormat(wand);
    if (str) {
        baton->ext = str;
        free(str);
    }
    // File extension, lowercase and 3 letters only
    std::transform(baton->ext.begin(), baton->ext.end(), baton->ext.begin(), ::tolower);
    if (baton->ext == "jpeg") baton->ext = "jpg";

    if (baton->out.size()) {
        // Make sure all subdirs exist
        if (bkMakePath(baton->out)) {
            string::size_type dot = baton->out.find_last_of('.');
            if (dot != string::npos) baton->out = baton->out.substr(0, dot);
            baton->out += "." + baton->ext;

            if (frames > 1) {
                status = MagickWriteImages(wand, baton->out.c_str(), MagickTrue);
            } else {
                status = MagickWriteImage(wand, baton->out.c_str());
            }
            if (status == MagickFalse) goto err;
        } else {
            baton->err = errno;
        }
    } else {
        if (frames > 1) {
            baton->image = MagickGetImagesBlob(wand, &baton->length);
        } else {
            baton->image = MagickGetImageBlob(wand, &baton->length);
        }
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

    Local<Value> argv[3];

    if (!baton->cb.IsEmpty()) {
        Local<Function> cb = Nan::New(baton->cb);
        if (baton->err || baton->exception) {
            argv[0] = Nan::Error(baton->err ? strerror(baton->err) : baton->exception);
            NAN_TRY_CATCH_CALL(Nan::GetCurrentContext()->Global(), cb, 1, argv);
        } else {
            Local<Object> info = Nan::New<Object>();
            info->Set(Nan::New("ext").ToLocalChecked(), Nan::New(baton->ext).ToLocalChecked());
            if (baton->out.size()) info->Set(Nan::New("file").ToLocalChecked(), Nan::New(baton->out).ToLocalChecked());
            if (baton->d.orientation) {
                info->Set(Nan::New("orientation").ToLocalChecked(), Nan::New(getMagickOrientation(baton->d.orientation)).ToLocalChecked());
                info->Set(Nan::New("rotation").ToLocalChecked(), Nan::New(getMagickAngle(baton->d.orientation)));
            }
            info->Set(Nan::New("width").ToLocalChecked(), Nan::New(baton->d.width));
            info->Set(Nan::New("height").ToLocalChecked(), Nan::New(baton->d.height));
            info->Set(Nan::New("_width").ToLocalChecked(), Nan::New(baton->o.width));
            info->Set(Nan::New("_height").ToLocalChecked(), Nan::New(baton->o.height));
            if (baton->o.ext.size()) info->Set(Nan::New("_ext").ToLocalChecked(), Nan::New(baton->o.ext).ToLocalChecked());
            if (baton->o.orientation) {
                info->Set(Nan::New("_orientation").ToLocalChecked(), Nan::New(getMagickOrientation(baton->o.orientation)).ToLocalChecked());
                info->Set(Nan::New("_rotation").ToLocalChecked(), Nan::New(getMagickAngle(baton->o.orientation)));
            }
            if (baton->image) {
                argv[0] = Nan::Null();
                argv[1] = Nan::CopyBuffer((const char*)baton->image, baton->length).ToLocalChecked();
                argv[2] = info;
                NAN_TRY_CATCH_CALL(Nan::GetCurrentContext()->Global(), cb, 3, argv);
            } else {
                argv[0] = Nan::Null();
                argv[1] = Nan::Null();
                argv[2] = info;
                NAN_TRY_CATCH_CALL(Nan::GetCurrentContext()->Global(), cb, 3, argv);
            }
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

    uv_work_t *req = new uv_work_t;
    MagickBaton *baton = new MagickBaton;
    req->data = baton;
    if (info.Length() > 2 && info[2]->IsFunction()) {
        baton->cb.Reset(Local<Function>::Cast(info[2]));
    }

    const Local<Array> names = opts->GetPropertyNames();
    for (uint i = 0 ; i < names->Length(); ++i) {
        String::Utf8Value key(names->Get(i));
        String::Utf8Value val(opts->Get(names->Get(i))->ToString());
        if (!strcmp(*key, "posterize")) baton->d.posterize = atoi(*val); else
        if (!strcmp(*key, "dither")) baton->d.dither = (DitherMethod)atoi(*val); else
        if (!strcmp(*key, "normalize")) baton->d.normalize = (DitherMethod)atoi(*val); else
        if (!strcmp(*key, "quantize")) baton->d.quantize = atoi(*val); else
        if (!strcmp(*key, "treedepth")) baton->d.tree_depth = atoi(*val); else
        if (!strcmp(*key, "flip")) baton->d.flip = atoi(*val); else
        if (!strcmp(*key, "strip")) baton->d.strip = atoi(*val); else
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
        if (!strcmp(*key, "no_animation")) baton->d.no_animation = atof(*val); else
        if (!strcmp(*key, "frame")) baton->d.frame = atof(*val); else
        if (!strcmp(*key, "opacity")) baton->d.rotate = atof(*val); else
        if (!strcmp(*key, "crop_width")) baton->d.crop_width = atoi(*val); else
        if (!strcmp(*key, "crop_height")) baton->d.crop_height = atoi(*val); else
        if (!strcmp(*key, "crop_x")) baton->d.crop_x = atoi(*val); else
        if (!strcmp(*key, "crop_y")) baton->d.crop_y = atoi(*val); else
        if (!strcmp(*key, "bgcolor")) baton->bgcolor = *val; else
        if (!strcmp(*key, "outfile")) baton->out = *val; else
        if (!strcmp(*key, "ext")) baton->format = *val; else
        if (!strcmp(*key, "filter")) baton->filter = getMagickFilter(*val); else
        if (!strcmp(*key, "gravity")) baton->d.gravity = getMagickGravity(*val);
        if (!strcmp(*key, "colorspace")) baton->d.colorspace = getMagickColorspace(*val);
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

static NAN_MODULE_INIT(WandInit)
{
    Nan::HandleScope scope;

    MagickWandGenesis();
    NAN_EXPORT(target, resizeImage);
}
#else
static NAN_MODULE_INIT(WandInit)
{
}
#endif

NODE_MODULE(binding, WandInit);

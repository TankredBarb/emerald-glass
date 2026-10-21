// SPDX-License-Identifier: LGPL-3.0-or-later
#include <epoxy/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <cstdlib>
#include <cmath>
void require(bool ok, const char *message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
GLuint shader(GLenum type, const std::string &source) {
    GLuint s=glCreateShader(type); const char *p=source.c_str(); glShaderSource(s,1,&p,nullptr); glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok); char log[8192]; glGetShaderInfoLog(s,sizeof(log),nullptr,log); if(!ok) std::cerr<<log;
    require(ok,"shader compilation"); return s;
}
int main(int argc,char **argv) {
    require(argc>=2,"shader path required");
    auto platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    require(platform,"EGL platform support");
    EGLDisplay d=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
    require(eglInitialize(d,nullptr,nullptr),"EGL initialize"); require(eglBindAPI(EGL_OPENGL_API),"OpenGL API");
    EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig cfg; EGLint n; require(eglChooseConfig(d,attrs,&cfg,1,&n)&&n,"EGL config");
    EGLint ca[]={EGL_CONTEXT_MAJOR_VERSION,3,EGL_CONTEXT_MINOR_VERSION,3,EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
    EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca); require(c!=EGL_NO_CONTEXT,"EGL context");
    EGLint pa[]={EGL_WIDTH,256,EGL_HEIGHT,256,EGL_NONE}; EGLSurface surf=eglCreatePbufferSurface(d,cfg,pa);
    require(eglMakeCurrent(d,surf,surf,c),"EGL current"); std::cout<<glGetString(GL_RENDERER)<<'\n';
    std::ifstream f(argv[1]); std::string fragment((std::istreambuf_iterator<char>(f)),{});
    const std::string vertex="#version 330 core\nout vec2 uv; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=p; gl_Position=vec4(p*2.0-1.0,0,1); }";
    GLuint prog=glCreateProgram(); glAttachShader(prog,shader(GL_VERTEX_SHADER,vertex)); glAttachShader(prog,shader(GL_FRAGMENT_SHADER,fragment)); glLinkProgram(prog);
    GLint ok; glGetProgramiv(prog,GL_LINK_STATUS,&ok); require(ok,"shader link"); glUseProgram(prog);
    auto loc=[&](const char *name){return glGetUniformLocation(prog,name);};
    auto one=[&](const char *name,float v){glUniform1f(loc(name),v);};
    auto count=[&](int v){glUniform1i(loc("buttonCount"),v);};
    GLuint vao; glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    GLuint texture; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    float background[]={.25f,.30f,.35f,1.f}; glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,background);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glUniform1i(loc("scene"),0); glUniform2f(loc("resolution"),128,128);
    glUniform4f(loc("frameRect"),0,0,128,128); glUniform4f(loc("contentRect"),8,40,112,80);
    glUniform3f(loc("accent"),.1,.9,.3); glUniform2f(loc("pointer"),20,15);
    one("refraction",.85); one("tint",.7); one("dispersion",.25); one("adaptiveContrast",.35);
    one("frameRadius",5); one("titleHeight",39); one("scale",1); one("frameOpacity",.84);
    glUniform1i(loc("kwinMode"),1); glUniform1i(loc("hoverButton"),-1); glUniform1i(loc("pressedButton"),-1); glUniform1i(loc("buttonCloseIndex"),2);
    auto render=[&](int side=128){ glViewport(0,0,side,side); glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLES,0,3); std::vector<unsigned char> out(side*side*4); glReadPixels(0,0,side,side,GL_RGBA,GL_UNSIGNED_BYTE,out.data()); require(glGetError()==GL_NO_ERROR,"GL error"); return out; };
    const std::array<std::array<float,3>,8> buttonPresets={{{23,245,89},{247,20,14},{20,168,68},{30,143,255},
                                                          {255,158,20},{184,51,255},{168,228,245},{255,140,157}}};
    if(argc>=4 && std::string(argv[2])=="--capture-buttons") {
        const float backdrop[]={.10f,.32f,.38f,1.f};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,backdrop);
        count(3); glUniform1i(loc("buttonCloseIndex"),2);
        glUniform4f(loc("buttonRect[0]"),15,8,27,23); glUniform4f(loc("buttonRect[1]"),45,8,27,23);
        glUniform4f(loc("buttonRect[2]"),75,8,27,23);
        one("frameOpacity",.32); one("tint",1.02); one("refraction",.45); one("dispersion",.60);
        one("specularStrength",.58); one("hoverGlowStrength",.88); one("adaptiveContrast",.52);
        for(int preset=0;preset<8;++preset) for(int state=0;state<4;++state) for(float dpr : {1.f,1.5f,2.f}) {
            glUniform3f(loc("accent"),buttonPresets[preset][0]/255,buttonPresets[preset][1]/255,buttonPresets[preset][2]/255);
            glUniform1i(loc("hoverButton"),state==1 ? 1 : state==2 ? 2 : -1);
            glUniform1i(loc("pressedButton"),state==3 ? 2 : -1);
            glUniform2f(loc("pointer"),state==1 ? 58.5f : 88.5f,19.5f); one("scale",dpr);
            const int side=int(128*dpr); const auto pixels=render(side);
            std::ofstream file(std::string(argv[3])+"/"+std::to_string(preset)+"-"+std::to_string(state)+"-"+std::to_string(int(dpr*100))+".ppm",std::ios::binary);
            file<<"P6\n"<<side<<' '<<side<<"\n255\n";
            for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
                const int i=((side-1-y)*side+x)*4;
                for(int k=0;k<3;++k) {
                    const unsigned char value=std::min(255,int(pixels[i+k])+int(backdrop[k]*(255-pixels[i+3])));
                    file.write(reinterpret_cast<const char *>(&value),1);
                }
            }
            require(bool(file),"button material capture saved");
        }
        return 0;
    }
    count(0); one("specularStrength",0); one("hoverGlowStrength",0); auto noLight=render();
    one("specularStrength",1); auto spec=render(); require(spec!=noLight,"zero specular must differ from 100 percent");
    one("specularStrength",0); one("hoverGlowStrength",1); auto glow=render(); require(glow==noLight,"native cursor overlay must not be duplicated inside KWin material"); count(1); glUniform4f(loc("buttonRect[0]"),5,5,27,23); glUniform1i(loc("hoverButton"),0); one("hoverGlowStrength",0); auto darkButton=render(); one("hoverGlowStrength",1); require(render()!=darkButton,"GPU button hover remains responsive"); count(0); glUniform1i(loc("hoverButton"),-1);
    one("hoverGlowStrength",0); count(3);
    glUniform4f(loc("buttonRect[0]"),15,8,27,23); glUniform4f(loc("buttonRect[1]"),45,8,27,23); glUniform4f(loc("buttonRect[2]"),75,8,27,23);
    auto buttons=render(); require(buttons!=noLight,"buttons render lenses"); count(0); require(render()==noLight,"zero buttons must ignore old rectangles");
    one("frameOpacity",.2); auto low=render(); one("frameOpacity",.8); auto high=render();
    const int pixel=((128-1-20)*128+10)*4;
    require(low[pixel+3]==255 && high[pixel+3]==255,"blur fully replaces the original backdrop at every material opacity"); require(low!=high,"material opacity still affects tint and optics");
    for(int i=0;i<int(low.size());i+=4) for(int j=0;j<3;++j) require(low[i+j]<=low[i+3],"premultiplied frame RGB");
    require(high[3]==0,"outside corner remains transparent");
    count(3); auto composited=render();
    for(int i=0;i<int(composited.size());i+=4) for(int j=0;j<3;++j) require(composited[i+j]<=composited[i+3],"premultiplied button composition");
    // High-contrast backdrop models text behind a clear titlebar.
    count(0); one("frameOpacity",0); one("refraction",0); one("dispersion",0);
    std::vector<unsigned char> stripes(128*128*4);
    for(int y=0;y<128;++y) for(int x=0;x<128;++x) {
        int i=(y*128+x)*4; stripes[i]=stripes[i+1]=stripes[i+2]=((x/4)%2)*255; stripes[i+3]=255;
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,stripes.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    one("backgroundBlurOffset",0); auto sharp=render();
    one("backgroundBlurOffset",3); auto soft=render();
    auto contrast=[](const auto &pixels){ double e=0; for(int y=108;y<118;++y) for(int x=10;x<118;++x) { double v=int(pixels[(y*128+x)*4])-127.5; e+=v*v; } return e; };
    require(contrast(soft)<contrast(sharp)*.75,"Kawase reconstruction softens text even at zero material opacity");
    // Lens controls must change the shoulder without moving the flat titlebar center.
    one("backgroundBlurOffset",0); one("frameOpacity",.35f); one("tint",0); one("specularStrength",0);
    one("dispersion",0); one("refraction",0); auto flat=render();
    one("refraction",1); auto lens=render();
    require(lens!=flat,"refraction bends the background");
    const int center=((127-20)*128+60)*4;
    for(int k=0;k<4;++k) require(lens[center+k]==flat[center+k],"flat titlebar center stays optically stable");
    one("dispersion",1); require(render()!=lens,"dispersion produces edge color separation");
    one("refraction",0); require(render()==flat,"zero refraction disables chromatic displacement");
    one("frameOpacity",0); one("dispersion",0); auto clearFlat=render(); one("refraction",1);
    require(render()!=clearFlat,"clear glass keeps its lens instead of fading optics with opacity");

    // The caption adaptation must work with both tint and opacity at zero.
    count(0); one("tint",0); one("frameOpacity",0); one("refraction",0); one("dispersion",0);
    one("specularStrength",.57f); one("backgroundBlurOffset",0);
    auto backdrop=[&](float level) {
        const float pixel[]={level,level,level,1};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,pixel);
    };
    auto captionLuma=[](const auto &image) {
        double total=0; int samples=0;
        for(int y=12;y<29;++y) for(int x=10;x<118;++x) {
            const int i=((127-y)*128+x)*4;
            total+=.299*image[i]+.587*image[i+1]+.114*image[i+2]; ++samples;
        }
        return total/samples;
    };
    backdrop(.95f); one("adaptiveContrast",0); const auto brightOff=render();
    one("adaptiveContrast",.5f); const auto brightHalf=render();
    one("adaptiveContrast",1); const auto brightOn=render();
    require(captionLuma(brightOn)<captionLuma(brightHalf) && captionLuma(brightHalf)<captionLuma(brightOff)-10,
            "adaptive contrast progressively darkens bright captions independently of tint and opacity");
    backdrop(.05f); one("adaptiveContrast",0); const auto darkOff=render();
    one("adaptiveContrast",1); const auto darkOn=render();
    require(std::abs(captionLuma(darkOn)-captionLuma(darkOff))<.1,
            "adaptive contrast preserves dark caption backgrounds");

    // A sharp lower window must never bypass the selected blur via a second sampler.
    backdrop(.25f); one("adaptiveContrast",0); one("refraction",1.8f); one("dispersion",1);
    one("backgroundBlurOffset",3);
    GLuint raw; glGenTextures(1,&raw); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,raw);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    const float green[]={0,1,0,1}, red[]={1,0,0,1};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,green);
    glUniform1i(loc("clearScene"),1); glActiveTexture(GL_TEXTURE0);
    const auto lowerGreen=render();
    glActiveTexture(GL_TEXTURE1); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,red);
    glActiveTexture(GL_TEXTURE0);
    require(render()==lowerGreen,"refraction and dispersion never reintroduce the sharp lower-window scene");
    glUniform1i(loc("opticsDisabled"),1); const auto plainBlur=render();
    one("adaptiveContrast",1); one("specularStrength",1.5f); one("refraction",0); one("dispersion",0);
    require(render()==plainBlur,"disabled optics retains neutral blur independent of optical controls");
    // Occlusion leaves transparent texels in the cached backdrop. Standard Blur
    // preserves that alpha; turning it opaque compounds black into every layer.
    one("backgroundBlurOffset",3);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    float stackColor[]={.3f,.55f,.25f,1};
    const float originalGreen=stackColor[1];
    for(int layer=0;layer<6;++layer) {
        const float partialBackdrop[]={stackColor[0]*.75f,stackColor[1]*.75f,stackColor[2]*.75f,.75f};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,partialBackdrop);
        glClearColor(stackColor[0],stackColor[1],stackColor[2],1); glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES,0,3);
        unsigned char centerPixel[4]; glReadPixels(60,107,1,1,GL_RGBA,GL_UNSIGNED_BYTE,centerPixel);
        for(int k=0;k<3;++k) stackColor[k]=centerPixel[k]/255.f;
        std::cout<<"Neutral blur stack layer "<<layer+1<<": green="<<centerPixel[1]+0<<'\n';
    }
    require(std::abs(stackColor[1]-originalGreen)<2.f/255,
            "neutral blur preserves backdrop brightness through six partially cached layers");
    glDisable(GL_BLEND);
    // At a flat, unlit caption, missing cache coverage must leave the same
    // backdrop contribution visible rather than introduce black. Exercise both
    // frame and button and preserve the 100% opacity contract.
    glUniform1i(loc("opticsDisabled"),0);
    one("tint",0); one("specularStrength",0); one("adaptiveContrast",0);
    one("refraction",0); one("dispersion",0); one("backgroundBlurOffset",0);
    const float base[]={.3f,.55f,.25f,1};
    for(int buttons : {0,1}) {
        count(buttons); glUniform4f(loc("buttonRect[0]"),45,8,27,23);
        for(float opacity : {0.f,.2f,.8f,1.f}) {
            one("frameOpacity",opacity);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,base);
            const auto complete=render();
            for(float valid : {.75f,0.f}) {
                const float partial[]={base[0]*valid,base[1]*valid,base[2]*valid,valid};
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,partial);
                const auto image=render();
                for(int k=0;k<3;++k) {
                    if(buttons==0) {
                        float composed=image[center+k]+base[k]*(255-image[center+3]);
                        require(std::abs(composed-complete[center+k])<2,"LG partial-cache composition preserves flat backdrop brightness");
                    }
                    require(image[center+k]<=image[center+3],"LG partial-cache material stays premultiplied");
                }
                require(opacity<1 || image==complete,"opaque LG excludes missing backdrop coverage for frame and button");
            }
        }
    }
    count(0); one("frameOpacity",.84f); one("backgroundBlurOffset",3);
    glUniform1i(loc("opticsDisabled"),1);
    const float colorful[]={.22f,.44f,.28f,1};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,colorful);
    one("backdropSaturationOffset",0); const auto saturation100=render();
    one("backdropSaturationOffset",.73f); const auto saturation173=render();
    auto chroma=[&](const auto &image) {
        return std::max({image[center],image[center+1],image[center+2]})-
               std::min({image[center],image[center+1],image[center+2]});
    };
    require(chroma(saturation173)>chroma(saturation100)*1.6,"basic border blur applies standard Blur saturation");
    require(saturation173[center+3]==saturation100[center+3],"backdrop saturation preserves alpha");
    glUniform1i(loc("opticsDisabled"),0); one("adaptiveContrast",0); one("specularStrength",0);
    one("refraction",0); one("dispersion",0); one("frameOpacity",.36f);
    one("tint",1); const auto tint100=render();
    one("tint",2); const auto tint200=render();
    require(chroma(tint200)>chroma(tint100),"shader extra tint visibly strengthens preset color");
    auto pixelLuma=[&](const auto &image){ return .299*image[center]+.587*image[center+1]+.114*image[center+2]; };
    require(std::abs(pixelLuma(tint200)-pixelLuma(tint100))<1.5,"shader extra tint preserves caption luminance");
    require(tint200[center+3]==tint100[center+3],"shader extra tint preserves alpha");
    count(1); glUniform4f(loc("buttonRect[0]"),5,5,27,23); glUniform1i(loc("buttonCloseIndex"),0);
    one("tint",0); one("hoverGlowStrength",0); one("specularStrength",1);
    const float greenBackdrop[]={.08f,.95f,.12f,1};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,greenBackdrop);
    glUniform1i(loc("hoverButton"),-1); glUniform1i(loc("pressedButton"),-1); const auto idleClose=render();
    glUniform1i(loc("buttonCloseIndex"),1); require(render()==idleClose,"idle GPU close uses the common button material");
    glUniform1i(loc("buttonCloseIndex"),0);
    const int buttonPixel=((127-18)*128+18)*4;
    for(bool pressed : {false,true}) {
        glUniform1i(loc("hoverButton"),pressed ? -1 : 0); glUniform1i(loc("pressedButton"),pressed ? 0 : -1);
        const auto redClose=render();
        require(redClose[buttonPixel]>redClose[buttonPixel+1]+20 && redClose[buttonPixel]>redClose[buttonPixel+2]+20,
                "GPU close hover and press retain a red center over green at zero tint and glow");
    }
    for(const auto &baseColor : {std::array<float,4>{.97f,.08f,.055f,1},
                                std::array<float,4>{1,.55f,.62f,1},
                                std::array<float,4>{.47f,.13f,.18f,1},
                                std::array<float,4>{1,.62f,.08f,2},
                                std::array<float,4>{.09f,.96f,.35f,0}}) {
        glUniform3f(loc("accent"),baseColor[0],baseColor[1],baseColor[2]);
        for(bool pressed : {false,true}) for(float glow : {0.f,1.f}) {
            one("hoverGlowStrength",glow);
            glUniform1i(loc("hoverButton"),pressed ? -1 : 0); glUniform1i(loc("pressedButton"),pressed ? 0 : -1);
            const auto pixels=render();
            const int r=pixels[buttonPixel],g=pixels[buttonPixel+1],b=pixels[buttonPixel+2];
            // Colored glass transmits this deliberately bright green backdrop.
            // Require a visible channel margin rather than an opaque 2:1 fill.
            constexpr int signalMargin=12;
            require(baseColor[3]==2 ? b>r+signalMargin && b>g+signalMargin
                                   : r>b+signalMargin && (baseColor[3]>0 ? g>b+signalMargin && g>r*.4 : r>g+signalMargin),
                    "GPU close uses blue on amber, gold on red/pink and red on emerald across hover/press and glow settings");
        }
    }
    glUniform3f(loc("accent"),.1,.9,.3); one("hoverGlowStrength",0);
    // Base opacity must dominate refraction and dispersion, including at zero tint.
    glUniform1i(loc("hoverButton"),-1); glUniform1i(loc("pressedButton"),-1);
    one("refraction",1.8f); one("dispersion",1); one("adaptiveContrast",0);
    const float purpleBackdrop[]={.65f,.05f,.85f,1};
    for (float tintValue : {0.f,1.f,2.f}) {
        one("tint",tintValue);
        int previousDifference=100000;
        for (float opacity : {0.f,.5f,.9f,1.f}) {
            one("frameOpacity",opacity);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,greenBackdrop);
            const auto overGreen=render();
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,purpleBackdrop);
            const auto overPurple=render();
            int difference=0;
            for (int sample : {center,buttonPixel}) for(int k=0;k<3;++k)
                difference+=std::abs(int(overGreen[sample+k])-int(overPurple[sample+k]));
            require(difference<=previousDifference,"higher LG base opacity progressively suppresses backdrop color");
            previousDifference=difference;
            if(opacity==1) require(overGreen==overPurple,"100 percent LG opacity excludes the entire backdrop even with maximum optics");
            else require(difference>0,"partial LG opacity keeps a transmitted backdrop");
        }
    }
    if(argc>=3 && (std::string(argv[2])=="--sheet-geometry" || std::string(argv[2])=="--capture-material")) {
        count(0); one("frameOpacity",0); one("tint",0); one("specularStrength",0); one("adaptiveContrast",0);
        one("backgroundBlurOffset",0); one("dispersion",0); one("refraction",0);
        for(int y=0;y<128;++y) for(int x=0;x<128;++x) {
            const int i=(y*128+x)*4;
            stripes[i]=stripes[i+1]=stripes[i+2]=((x/4+y/4)%2)*255; stripes[i+3]=255;
        }
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,stripes.data());
        const auto plateFlat=render(); one("refraction",1.8f); const auto plateBent=render();
        int edgeDelta=0;
        for(int y=2;y<10;++y) for(int x=20;x<105;++x) {
            const int i=((127-y)*128+x)*4;
            edgeDelta+=std::abs(int(plateFlat[i])-int(plateBent[i]));
        }
        if(std::string(argv[2])=="--sheet-geometry") {
        require(edgeDelta>10000,"sheet refraction produces visible outer-edge displacement");
        for(int y=18;y<39;++y) for(int x=20;x<105;++x) for(int k=0;k<4;++k) {
            const int i=((127-y)*128+x)*4+k;
            require(plateFlat[i]==plateBent[i],"sheet center and client-side title edge do not become a second lens");
        }
        }
        if(argc>=4) {
            auto capture=[&](const auto &pixels,const char *suffix) {
                std::ofstream file(std::string(argv[3])+suffix,std::ios::binary);
                file<<"P6\n128 128\n255\n";
                for(int y=0;y<128;++y) for(int x=0;x<128;++x) {
                    const int i=((127-y)*128+x)*4;
                    for(int k=0;k<3;++k) {
                        unsigned char value=pixels[i+k]+stripes[i+k]*(255-pixels[i+3])/255;
                        file.write(reinterpret_cast<const char *>(&value),1);
                    }
                }
                require(bool(file),"material capture saved");
            };
            capture(plateFlat,"-flat.ppm"); capture(plateBent,"-refracted.ppm");
            one("dispersion",1); capture(render(),"-dispersed.ppm");
            one("frameOpacity",1); capture(render(),"-opaque.ppm");
        }
        std::cout<<"Sheet outer-edge refraction delta="<<edgeDelta<<'\n';
    }
    // The user's Crystal settings exposed white hover washout and a clipped
    // Close body. Check the real shader at native and fractional pixel scales.
    const float tealBackdrop[]={.10f,.32f,.38f,1.f};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,tealBackdrop);
    glUniform2f(loc("resolution"),128,128); glUniform4f(loc("frameRect"),0,0,128,128);
    glUniform4f(loc("contentRect"),8,40,112,80); count(3);
    glUniform1i(loc("kwinMode"),1); glUniform1i(loc("opticsDisabled"),0); glUniform1i(loc("buttonCloseIndex"),2);
    glUniform4f(loc("buttonRect[0]"),15,8,27,23); glUniform4f(loc("buttonRect[1]"),45,8,27,23);
    glUniform4f(loc("buttonRect[2]"),75,8,27,23);
    one("frameOpacity",.32); one("tint",1.02); one("refraction",.45); one("dispersion",.60);
    one("specularStrength",.58); one("hoverGlowStrength",.88); one("adaptiveContrast",.52);
    for(const auto &color : buttonPresets) for(float dpr : {1.f,1.25f,1.5f,2.f}) {
        glUniform3f(loc("accent"),color[0]/255,color[1]/255,color[2]/255); one("scale",dpr);
        const int side=int(128*dpr);
        glUniform2f(loc("pointer"),88.5,19.5);
        glUniform1i(loc("hoverButton"),-1); glUniform1i(loc("pressedButton"),-1);
        const auto idle=render(side);
        for(bool pressed : {false,true}) {
            glUniform1i(loc("hoverButton"),pressed ? -1 : 2); glUniform1i(loc("pressedButton"),pressed ? 2 : -1);
            const auto image=render(side);
            int saturated=0,samples=0;
            for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
                const int i=((side-1-y)*side+x)*4;
                const float lx=(x+.5f)/dpr,ly=(y+.5f)/dpr;
                if(lx>=76 && lx<101 && ly>=9 && ly<30) { ++samples; saturated+=std::max({image[i],image[i+1],image[i+2]})>=254; }
                if(lx<74 || lx>103 || ly<7 || ly>32) for(int k=0;k<4;++k)
                    require(image[i+k]==idle[i+k],"Close hover/press must not paint outside its lens");
            }
            require(saturated<samples/20,"Close lighting must retain its gradient instead of clipping into a flat plate");
        }
        const float emptyBackdrop[]={0,0,0,0};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,emptyBackdrop);
        one("frameOpacity",.2); one("specularStrength",1.5); one("hoverGlowStrength",1.5);
        glUniform1i(loc("hoverButton"),2); glUniform1i(loc("pressedButton"),-1);
        const auto partial=render(side);
        for(int i=0;i<int(partial.size());i+=4) for(int k=0;k<3;++k)
            require(partial[i+k]<=partial[i+3]+1,"bright button edges must retain premultiplied coverage");
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,tealBackdrop);
        one("frameOpacity",.32); one("specularStrength",.58); one("hoverGlowStrength",.88);
        if(color==buttonPresets[6]) {
            glUniform2f(loc("pointer"),58.5,19.5);
            glUniform1i(loc("pressedButton"),-1); glUniform1i(loc("hoverButton"),1);
            const auto image=render(side); int white=0,samples=0;
            for(int y=int(8*dpr);y<int(31*dpr);++y) for(int x=int(45*dpr);x<int(72*dpr);++x) {
                const int i=((side-1-y)*side+x)*4; ++samples;
                white+=std::min({image[i],image[i+1],image[i+2]})>205;
            }
            require(white<samples/50,"Crystal hover must preserve its blue tint rather than washing out white");
        }
    }
    std::cout<<"PASS: material controls, caption adaptation without tint, dark caption preservation, lower-window blur isolation, optics-off neutral blur and six-layer alpha preservation\n";
}

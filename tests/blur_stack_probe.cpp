// SPDX-License-Identifier: LGPL-3.0-or-later
// Diagnostic comparison of real blur shaders, not a KWin scene/lifecycle test.
#include "../native/material.h"
#include "../native/blurprofile.h"
#include <epoxy/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <QCoreApplication>
#include <QFile>
#include <iostream>
#include <vector>

static void require(bool ok,const char *why) { if(!ok) {std::cerr<<why<<'\n';std::exit(1);} }
static QByteArray read(const QString &path) {QFile f(path);require(f.open(QIODevice::ReadOnly),qPrintable(path));return f.readAll();}
static GLuint program(QByteArray source)
{
    source.replace("#version 140", "#version 330 core");
    source.replace("texture2D(","texture(");
    if(!source.startsWith("#version")) source.prepend("#version 330 core\nout vec4 fragColor;\n");
    const QByteArray vertex="#version 330 core\nout vec2 uv; out vec2 vertex; void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;vertex=p*vec2(512,180);gl_Position=vec4(p*2-1,0,1);}";
    GLuint result=glCreateProgram();
    for(auto type:{GL_VERTEX_SHADER,GL_FRAGMENT_SHADER}) {
        GLuint shader=glCreateShader(type);const auto &text=type==GL_VERTEX_SHADER?vertex:source;
        const char *data=text.constData();glShaderSource(shader,1,&data,nullptr);glCompileShader(shader);
        GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);char log[8192];glGetShaderInfoLog(shader,sizeof(log),nullptr,log);
        if(!ok) std::cerr<<log;require(ok,"shader compile");glAttachShader(result,shader);glDeleteShader(shader);
    }
    glLinkProgram(result);GLint ok;glGetProgramiv(result,GL_LINK_STATUS,&ok);require(ok,"program link");return result;
}
static void scalar(GLuint p,const char *name,float v) {glUniform1f(glGetUniformLocation(p,name),v);}
static void integer(GLuint p,const char *name,int v) {glUniform1i(glGetUniformLocation(p,name),v);}
struct Target {GLuint texture=0,fbo=0;int w,h;};
static Target target(int w,int h)
{
    Target t{0,0,w,h};glGenTextures(1,&t.texture);glBindTexture(GL_TEXTURE_2D,t.texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1,&t.fbo);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.texture,0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"framebuffer");return t;
}
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    require(argc>=3,"usage: blur-stack-probe REPO {STOCK_SHADER_DIR OUTPUT_DIR | --header-optics [OUTPUT_DIR]}");
    const bool headerOptics=QString::fromLocal8Bit(argv[2])==QLatin1String("--header-optics");
    require(headerOptics || argc==4,"stock shader comparison requires an output directory");
    const QString root=QString::fromLocal8Bit(argv[1]),stock=QString::fromLocal8Bit(argv[2]);
    const QString out=argc>=4 ? QString::fromLocal8Bit(argv[3]) : QString();
    auto platform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    require(platform,"EGL surfaceless");const auto display=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
    require(eglInitialize(display,nullptr,nullptr)&&eglBindAPI(EGL_OPENGL_API),"EGL init");
    EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};EGLConfig config;EGLint count;
    require(eglChooseConfig(display,attrs,&config,1,&count)&&count,"EGL config");
    EGLint ca[]={EGL_CONTEXT_MAJOR_VERSION,3,EGL_CONTEXT_MINOR_VERSION,3,EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
    auto context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);EGLint pa[]={EGL_WIDTH,512,EGL_HEIGHT,180,EGL_NONE};
    auto surface=eglCreatePbufferSurface(display,config,pa);require(eglMakeCurrent(display,surface,surface,context),"EGL current");
    std::cout<<glGetString(GL_RENDERER)<<'\n';GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    const auto vendor=root+"/vendor/kwin-effects-glass/src/shaders/";
    GLuint ownDown=program(read(vendor+"downsample.glsl")),ownUp=program(read(vendor+"upsample.glsl"));
    GLuint ownFinal=program(read(root+"/preview/glass.frag"));
    std::vector<Target> pool;for(int level=0;level<5;++level) pool.push_back(target(512>>level,180>>level));
    const auto output=target(512,180);
    auto pass=[&](GLuint p,int source,int destination,float offset) {
        glBindFramebuffer(GL_FRAMEBUFFER,pool[destination].fbo);glViewport(0,0,pool[destination].w,pool[destination].h);
        glUseProgram(p);integer(p,"texUnit",0);scalar(p,"offset",offset);scalar(p,"saturationCompensation",1);
        glUniform2f(glGetUniformLocation(p,"halfpixel"),.5f/pool[source].w,.5f/pool[source].h);
        glBindTexture(GL_TEXTURE_2D,pool[source].texture);glDrawArrays(GL_TRIANGLES,0,3);
    };
    if(headerOptics) {
        // Broad colored transitions survive realistic Kawase blur; fine detail alone
        // would make a slider test pass only with the blur disabled.
        QImage backdrop(512,180,QImage::Format_RGBA8888);
        for(int y=0;y<180;++y) for(int x=0;x<512;++x) {
            const float wave=0.5f+0.5f*std::sin((x+y*2.0f)*0.035f);
            backdrop.setPixelColor(x,y,QColor(int(35+wave*185),int(210-wave*155),int(45+(1-wave)*170)));
            if((x+y)%32<3) backdrop.setPixelColor(x,y,QColor(235,245,255));
        }
        bool greenMaterial=false;
        auto render=[&](int strength,float refraction,float dispersion) {
            const auto profile=EmeraldGlass::blurProfile(strength);
            const float offset=profile.offset*.89f;
            const auto upload=backdrop.flipped(Qt::Vertical);glBindTexture(GL_TEXTURE_2D,pool[0].texture);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,512,180,GL_RGBA,GL_UNSIGNED_BYTE,upload.constBits());
            glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);
            for(int i=1;i<=profile.iterations;++i) pass(ownDown,i-1,i,offset);
            for(int i=profile.iterations;i>1;--i) pass(ownUp,i,i-1,offset);
            glBindFramebuffer(GL_FRAMEBUFFER,output.fbo);glViewport(0,0,512,180);glUseProgram(ownFinal);
            integer(ownFinal,"scene",0);integer(ownFinal,"kwinMode",1);integer(ownFinal,"opticsDisabled",0);
            integer(ownFinal,"buttonCount",0);integer(ownFinal,"hoverButton",-1);integer(ownFinal,"pressedButton",-1);
            scalar(ownFinal,"scale",1);scalar(ownFinal,"frameRadius",5);scalar(ownFinal,"titleHeight",39);
            scalar(ownFinal,"frameOpacity",greenMaterial ? .32f : .35f);scalar(ownFinal,"tint",greenMaterial ? 1.02f : .35f);scalar(ownFinal,"specularStrength",greenMaterial ? .59f : .65f);
            scalar(ownFinal,"adaptiveContrast",greenMaterial ? .51f : .35f);scalar(ownFinal,"refraction",refraction);scalar(ownFinal,"dispersion",dispersion);
            scalar(ownFinal,"backgroundBlurOffset",offset);
            glUniform2f(glGetUniformLocation(ownFinal,"resolution"),512,180);
            glUniform4f(glGetUniformLocation(ownFinal,"frameRect"),0,0,512,180);
            glUniform4f(glGetUniformLocation(ownFinal,"contentRect"),8,39,496,133);
            glUniform3f(glGetUniformLocation(ownFinal,"accent"),greenMaterial ? 20.f/255 : .09f,greenMaterial ? 168.f/255 : .96f,greenMaterial ? 68.f/255 : .35f);
            glBindTexture(GL_TEXTURE_2D,pool[profile.iterations>0 ? 1 : 0].texture);glDrawArrays(GL_TRIANGLES,0,3);
            QImage result(512,180,QImage::Format_RGBA8888);glReadPixels(0,0,512,180,GL_RGBA,GL_UNSIGNED_BYTE,result.bits());
            require(glGetError()==GL_NO_ERROR,"header optics pipeline");return result.flipped(Qt::Vertical);
        };
        auto difference=[](const QImage &a,const QImage &b) {
            double delta=0;
            for(int y=2;y<12;++y) for(int x=20;x<492;++x) {
                const auto ca=a.pixelColor(x,y),cb=b.pixelColor(x,y);
                delta+=std::abs(ca.red()-cb.red())+std::abs(ca.green()-cb.green())+std::abs(ca.blue()-cb.blue());
            }
            return delta/(10*472*3);
        };
        for(int strength : {0,4,8}) {
            const auto flat=render(strength,0,0),half=render(strength,.9f,0),full=render(strength,1.8f,0);
            const auto colorHalf=render(strength,1.8f,.5f),colorFull=render(strength,1.8f,1);
            const double ref=difference(flat,full),refFirst=difference(flat,half),refLast=difference(half,full);
            const double disp=difference(full,colorFull),dispFirst=difference(full,colorHalf),dispLast=difference(colorHalf,colorFull);
            std::cout<<"39px header blur="<<strength<<" refraction RGB mean="<<ref<<" halves="<<refFirst<<","<<refLast
                     <<" dispersion RGB mean="<<disp<<" halves="<<dispFirst<<","<<dispLast<<'\n';
            require(ref>4 && refFirst>1 && refLast>1,"refraction must visibly affect both halves of the header control range");
            require(disp>4 && dispFirst>1 && dispLast>1,"dispersion must visibly affect both halves of the header control range");
            require(render(strength,0,1)==flat,"zero refraction disables spectral displacement");
            for(int y=20;y<39;++y) for(int x=20;x<492;++x)
                require(flat.pixel(x,y)==colorFull.pixel(x,y),"header caption body remains stable");
            require(full.pixelColor(50,50).alpha()==0,"optics excludes client content");
            if(!out.isEmpty()) {
                require(flat.copy(0,0,512,39).save(out+QStringLiteral("/header-blur%1-flat.png").arg(strength)),"header capture");
                require(full.copy(0,0,512,39).save(out+QStringLiteral("/header-blur%1-refraction.png").arg(strength)),"header capture");
                require(colorFull.copy(0,0,512,39).save(out+QStringLiteral("/header-blur%1-dispersion.png").arg(strength)),"header capture");
            }
        }
        greenMaterial=true;
        // A uniform green scene reproduces why shifting blurred green pixels
        // cannot reveal the old controls, despite a passing colorful fixture.
        backdrop.fill(QColor(40,145,75));
        const auto greenFlat=render(7,0,0),greenHalf=render(7,.9f,0),greenFull=render(7,1.8f,0);
        const auto greenColorHalf=render(7,1.8f,.5f),greenColorFull=render(7,1.8f,1);
        const auto greenCurrent=render(7,.78f,.33f);
        std::cout<<"Uniform green / current material settings: refraction="<<difference(greenFlat,greenFull)
                 <<" halves="<<difference(greenFlat,greenHalf)<<","<<difference(greenHalf,greenFull)
                 <<" dispersion="<<difference(greenFull,greenColorFull)
                 <<" halves="<<difference(greenFull,greenColorHalf)<<","<<difference(greenColorHalf,greenColorFull)<<'\n';
        if(!out.isEmpty()) {
            require(greenFlat.copy(0,0,512,39).save(out+"/green-flat.png"),"green header capture");
            require(greenFull.copy(0,0,512,39).save(out+"/green-refraction.png"),"green header capture");
            require(greenColorFull.copy(0,0,512,39).save(out+"/green-dispersion.png"),"green header capture");
            require(greenCurrent.copy(0,0,512,39).save(out+"/green-current-settings.png"),"green header capture");
        }
        require(difference(greenFlat,greenFull)>4 && difference(greenFlat,greenHalf)>1 && difference(greenHalf,greenFull)>1,
                "refraction must shape the glass edge even over a uniform green scene");
        require(difference(greenFull,greenColorFull)>4 && difference(greenFull,greenColorHalf)>1 && difference(greenColorHalf,greenColorFull)>1,
                "dispersion must split edge reflections even over a uniform green scene");
        for(int y=18;y<39;++y) for(int x=20;x<492;++x)
            require(greenFlat.pixel(x,y)==greenColorFull.pixel(x,y),"surface control changes leave the green caption body intact");
        // Check the second control at ordinary refraction, not only at 180%.
        for(float refraction : {.78f,.4f,1.2f}) {
            const auto noSplit=render(7,refraction,0),halfSplit=render(7,refraction,.5f),fullSplit=render(7,refraction,1);
            const double fullDelta=difference(noSplit,fullSplit),firstDelta=difference(noSplit,halfSplit),lastDelta=difference(halfSplit,fullSplit);
            std::cout<<"Green header refraction="<<refraction<<" dispersion="<<fullDelta<<" halves="<<firstDelta<<","<<lastDelta<<'\n';
            if(!out.isEmpty()) for(int percent : {0,25,50,75,100})
                require(render(7,refraction,percent/100.f).copy(0,0,512,39).save(out+QStringLiteral("/green-ref%1-disp%2.png").arg(qRound(refraction*100)).arg(percent)),"ordinary refraction dispersion capture");
            require(fullDelta>8 && firstDelta>2 && lastDelta>2,
                    "dispersion must remain clearly visible without maximum refraction");
            for(int y=18;y<39;++y) for(int x=20;x<492;++x)
                require(noSplit.pixel(x,y)==fullSplit.pixel(x,y),"ordinary-refraction dispersion preserves the caption body");
        }
        return 0;
    }
    GLuint stockDown=program(read(stock+"/downsample.frag")),stockUp=program(read(stock+"/upsample.frag"));
    auto finalSource=read(stock+"/onscreen_rounded.frag");finalSource.replace("#include \"sdf.glsl\"",read(root+"/vendor/kwin-sdf-6.7.5.glsl"));
    GLuint stockFinal=program(finalSource);
    EmeraldGlass::MaterialConfig cfg;cfg.refraction=.6f;cfg.specular=.39f;cfg.dispersion=.07f;cfg.tint=cfg.inactiveTint=.5f;cfg.frameOpacity=.36f;cfg.shaderEffectEnabled=false;
    const QRectF frame(0,0,512,180),client(8,39,496,133);
    const auto coat=EmeraldGlass::renderFrameGlassImage(QSize(512,180),1,frame,client,QColor(20,168,68),cfg.tint,5,cfg);
    struct Mode {const char *name;bool stock;float saturation;};
    for(const auto mode:{Mode{"stock100",true,1},Mode{"stock173",true,1.73f},Mode{"ours100",false,1},Mode{"ours173",false,1.73f}}) {
        QImage scene(512,180,QImage::Format_RGBA8888);scene.fill(QColor(56,145,85));
        for(int layer=1;layer<=6;++layer) {
            const auto upload=scene.flipped(Qt::Vertical);glBindTexture(GL_TEXTURE_2D,pool[0].texture);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,512,180,GL_RGBA,GL_UNSIGNED_BYTE,upload.constBits());
            glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);
            for(int i=1;i<=3;++i) pass(mode.stock?stockDown:ownDown,i-1,i,4.4f);
            for(int i=3;i>1;--i) pass(mode.stock?stockUp:ownUp,i,i-1,4.4f);
            glBindFramebuffer(GL_READ_FRAMEBUFFER,pool[0].fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,output.fbo);
            glBlitFramebuffer(0,0,512,180,0,0,512,180,GL_COLOR_BUFFER_BIT,GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER,output.fbo);glViewport(0,0,512,180);
            const GLuint p=mode.stock?stockFinal:ownFinal;glUseProgram(p);
            integer(p,"texUnit",0);integer(p,"scene",0);integer(p,"kwinMode",1);integer(p,"opticsDisabled",1);
            scalar(p,"offset",4.4f);scalar(p,"backgroundBlurOffset",4.4f);scalar(p,"opacity",1);scalar(p,"scale",1);scalar(p,"frameRadius",5);
            glUniform2f(glGetUniformLocation(p,"resolution"),512,180);glUniform2f(glGetUniformLocation(p,"halfpixel"),.5f/pool[1].w,.5f/pool[1].h);
            glUniform4f(glGetUniformLocation(p,"frameRect"),0,0,512,180);glUniform4f(glGetUniformLocation(p,"contentRect"),8,39,496,133);
            glUniform4f(glGetUniformLocation(p,"box"),256,90,256,90);glUniform4f(glGetUniformLocation(p,"cornerRadius"),5,5,5,5);
            float a=(1-mode.saturation)*.2126f,b=(1-mode.saturation)*.7152f,c=(1-mode.saturation)*.0722f;
            const float matrix[]={a+mode.saturation,b,c,0,a,b+mode.saturation,c,0,a,b,c+mode.saturation,0,0,0,0,1};
            glUniformMatrix4fv(glGetUniformLocation(p,"colorMatrix"),1,GL_FALSE,matrix);
            scalar(p,"backdropSaturationOffset",mode.saturation-1);
            glBindTexture(GL_TEXTURE_2D,pool[1].texture);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_SCISSOR_TEST);
            for(const auto r:{QRect(0,0,512,39),QRect(0,39,8,141),QRect(504,39,8,141),QRect(8,172,496,8)}) {
                glScissor(r.x(),180-r.y()-r.height(),r.width(),r.height());glDrawArrays(GL_TRIANGLES,0,3);
            }
            glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);
            QImage result(512,180,QImage::Format_RGBA8888);glReadPixels(0,0,512,180,GL_RGBA,GL_UNSIGNED_BYTE,result.bits());scene=result.flipped(Qt::Vertical);
            require(glGetError()==GL_NO_ERROR,"GL pipeline error");
            QPainter painter(&scene);painter.drawImage(0,0,coat);painter.fillRect(client,QColor(235,235,235));painter.end();
            double rgb[3]={};for(int y=17;y<25;++y) for(int x=32;x<480;++x) {auto pixel=scene.pixelColor(x,y);rgb[0]+=pixel.red();rgb[1]+=pixel.green();rgb[2]+=pixel.blue();}
            for(auto &v:rgb) v/=(8*448);
            std::cout<<mode.name<<" layer="<<layer<<" RGB="<<rgb[0]<<","<<rgb[1]<<","<<rgb[2]<<" luma="<<.299*rgb[0]+.587*rgb[1]+.114*rgb[2]<<" chroma="<<std::max({rgb[0],rgb[1],rgb[2]})-std::min({rgb[0],rgb[1],rgb[2]})<<'\n';
            scene.save(out+"/"+mode.name+QString::number(layer)+".png");
        }
    }
}

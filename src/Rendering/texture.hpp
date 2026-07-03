#ifndef TEXTURE_H
#define TEXTURE_H

class Texture
{
public:
    explicit Texture(const char* path);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind(int unit) const;

private:
    unsigned int m_ID = 0;
};

#endif

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
    unsigned int id() const { return m_ID; }

private:
    unsigned int m_ID = 0;
};

#endif

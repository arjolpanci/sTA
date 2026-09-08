#ifndef TEXTURE_H
#define TEXTURE_H

#include <vector>

class Texture
{
public:
    // Files are flipped on load because the rest of the project authors UVs
    // bottom-up; glTF does not, hence the flag.
    explicit Texture(const char* path, bool flip = true);

    // Already-decoded pixels, for images that arrive embedded in a model file
    // rather than as a standalone file on disk. 1-4 channels.
    Texture(const unsigned char* pixels, int width, int height, int channels);

    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind(int unit) const;
    unsigned int id() const { return m_ID; }

private:
    void upload(const unsigned char* pixels, int width, int height, int channels);
    unsigned int m_ID = 0;
};

#endif

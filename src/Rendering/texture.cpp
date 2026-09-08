#include "texture.hpp"

#include <glad/glad.h>
#include <iostream>

#include <stb_image.h>

Texture::Texture(const char* path, bool flip)
{
    stbi_set_flip_vertically_on_load(flip);
    int width = 0, height = 0, nrChannels = 0;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (!data)
    {
        std::cout << "Failed to load texture at path: " << path << std::endl;
        return;
    }

    upload(data, width, height, nrChannels);
    stbi_image_free(data);
}

Texture::Texture(const unsigned char* pixels, int width, int height, int channels)
{
    upload(pixels, width, height, channels);
}

void Texture::upload(const unsigned char* pixels, int width, int height, int channels)
{
    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = channels == 4 ? GL_RGBA : channels == 3 ? GL_RGB : channels == 2 ? GL_RG : GL_RED;
    int alignment; glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glGenerateMipmap(GL_TEXTURE_2D);
}

Texture::~Texture()
{
    if (m_ID)
        glDeleteTextures(1, &m_ID);
}

void Texture::bind(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}

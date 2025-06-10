#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "timer.h"

typedef struct
{
    int id;
    int start_row;
    int end_row;
    uint8_t ***input;
    uint8_t ***output;
    int width;
    int height;
} ThreadData;

// Aloca matriz 3D
uint8_t ***alloc_image(int h, int w)
{
    uint8_t ***img = malloc(h * sizeof(uint8_t **));

    for (int linha = 0; linha < h; linha++)
    {
        img[linha] = malloc(w * sizeof(uint8_t *));
        for (int col = 0; col < w; col++)
        {
            img[linha][col] = calloc(3, sizeof(uint8_t));
        }
    }
    return img;
}

void free_image(uint8_t ***img, int h, int w)
{
    for (int linha = 0; linha < h; linha++)
    {
        for (int col = 0; col < w; col++)
        {
            free(img[linha][col]);
        }
        free(img[linha]);
    }
    free(img);
}

void load_image(uint8_t ***image, const char *path, int *w, int *h)
{
    int channels;
    uint8_t *data = stbi_load(path, w, h, &channels, 3);
    if (!data)
    {
        fprintf(stderr, "Erro ao carregar imagem.\n");
        exit(1);
    }

    for (int linha = 0; linha < *h; linha++)
    {
        for (int col = 0; col < *w; col++)
        {
            for (int c = 0; c < 3; c++)
            {
                image[linha][col][c] = data[(linha * (*w) + col) * 3 + c];
            }
        }
    }
    stbi_image_free(data);
}

void save_image(uint8_t ***image, int w, int h, const char *path)
{
    uint8_t *data = malloc(w * h * 3);
    for (int linha = 0; linha < h; linha++)
    {
        for (int col = 0; col < w; col++)
        {
            for (int c = 0; c < 3; c++)
            {
                data[(linha * w + col) * 3 + c] = image[linha][col][c];
            }
        }
    }
    stbi_write_png(path, w, h, 3, data, w * 3);
    free(data);
}

void apply_color_filter(uint8_t ***image, int start_row, int end_row, int width)
{
    for (int linha = start_row; linha < end_row; linha++)
    {
        for (int col = 0; col < width; col++)
        {
            printf("[%d][%d][0] = %d\n", linha, col, image[linha][col][0]);
            image[linha][col][1] = 0; // Remove verde
            image[linha][col][2] = 0; // Remove azul
        }
    }
}
void apply_laplacian_block(uint8_t ***input, uint8_t ***output, int start_row, int end_row, int width, int height)
{
    int kernel[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}};

    for (int linha = start_row; linha < end_row; linha++)
    {
        if (linha == 0 || linha == height - 1)
            continue;
        for (int col = 1; col < width - 1; col++)
        {
            for (int c = 0; c < 3; c++)
            {
                int sum = 0;
                for (int ky = -1; ky <= 1; ky++)
                {
                    for (int kx = -1; kx <= 1; kx++)
                    {
                        sum += kernel[ky + 1][kx + 1] * input[linha + ky][col + kx][c];
                    }
                }
                int value = input[linha][col][c] + sum;
                if (value < 0)
                    value = 0;
                if (value > 255)
                    value = 255;
                output[linha][col][c] = (uint8_t)value;
            }
        }
    }
}

void *thread_func(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    // Filtro de cor
    apply_color_filter(data->input, data->start_row, data->end_row, data->width);

    // Filtro Laplaciano
    apply_laplacian_block(data->input, data->output, data->start_row, data->end_row, data->width, data->height);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int width, height;
    double ini, fim, delta;
    if (argc != 4)
    {
        fprintf(stderr, "Uso: %s <input_image> <output_image> <num_threads>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    int num_threads = atoi(argv[3]);

    if (num_threads <= 0)
    {
        fprintf(stderr, "Número inválido de threads.\n");
        return 1;
    }

    GET_TIME(ini);

    // limite de 5000x5000
    uint8_t ***input_image = alloc_image(5000, 5000);

    load_image(input_image, input_path, &width, &height);
    printf("Imagem carregada: %d x %d\n", width, height);

    uint8_t ***output_image = alloc_image(height, width);

    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    int rows_per_thread = height / num_threads;

    printf("altura da imagem: %d\n", height);
    printf("Dividindo a imagem em %d threads de %d linhas cada\n", num_threads, rows_per_thread);
    for (int i = 0; i < num_threads; i++)
    {
        thread_data[i].id = i;
        thread_data[i].start_row = i * rows_per_thread;
        thread_data[i].end_row = (i == num_threads - 1) ? height : (i + 1) * rows_per_thread;
        thread_data[i].input = input_image;
        thread_data[i].output = output_image;
        thread_data[i].width = width;
        thread_data[i].height = height;

        pthread_create(&threads[i], NULL, thread_func, &thread_data[i]);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    save_image(output_image, width, height, output_path);
    printf("Imagem com filtro salva em: %s\n", output_path);

    free_image(input_image, height, width);
    free_image(output_image, height, width);

    GET_TIME(fim);
    delta = fim - ini;
    printf("Tempo de execução: %f segundos\n", delta);

    return 0;
}

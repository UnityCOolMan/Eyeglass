#include <GL/glut.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <conio.h>
#include <math.h>
#include <malloc.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>


typedef struct {
    char magic[4];
    int num_lumps;
    int directory_pos;
} PWADHeader;

typedef struct {
    char name[8];
    int lump_pos;
    int lump_size;
} LumpEntry;

// DOOM WAD file structures
typedef struct {
    char identifier[4];
    int num_lumps;
    int directory_offset;
} wad_header_t;

typedef struct {
    int file_pos;
    int size;
    char name[8];
} wad_directory_t;

// Image data structure
typedef struct {
    char name[9];          // 8 chars + null terminator
    unsigned char *data;   // Raw pixel data
    int width;             // Image width
    int height;            // Image height
    int size;              // Data size
    GLuint texture_id;     // OpenGL texture ID
    bool is_valid;         // Flag to indicate if image is valid
} wad_image_t;

// DOOM palette (RGB triplets)
unsigned char doom_palette[256][3];

// Global variables
wad_image_t *images = NULL;
int total_images = 0;
int scroll_position = 0;
int window_width = 800;
int window_height = 600;
int images_per_row = 12;
int image_size = 128;      // Display size for images
int image_padding = 10;
char window_title[256] = "DOOM WAD Image Viewer";
char status_message[256] = "";
char wad_filename[256] = "";
int current_page = 0;
int images_per_page = 0;
bool show_help = false;
bool show_file_selector = false;
bool show_folder_selector = false;
char available_wads[10][256]; // Store up to 10 WAD filenames
int num_available_wads = 0;
int selected_wad_index = 0;
// DOOM palette (RGB triplets)
unsigned char doom_palette[256][3];
bool palette_loaded = false;
// Global variables to support folder selection
char input_png_folder[1024] = {0};
char wad_output_name[256] = "output";
char output_wad_folder[1024] = {0};
int selected_input_field = 0;

// Function prototypes

void load_wad_file(const char *filename);
void unload_current_wad();
void load_doom_palette();
bool extract_palette_from_wad(const char *filename);
bool is_image_lump(char *name);
void create_texture_from_image(wad_image_t *image);
void detect_image_dimensions(wad_image_t *image);
void display();
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void special_keys(int key, int x, int y);
void mouse(int button, int state, int x, int y);
void draw_string(float x, float y, const char *text);
void find_available_wads();
void file_selector_menu();
int pngtopwad(const char* input_folder, const char* wad_name, const char* output_folder);
int read_png_dimensions(const char* filename, int* width, int* height);
void folder_selector_menu();
int bmp32_to_pwad(const char* input_folder, const char* wad_name, const char* output_folder);

int main(int argc, char** argv) {
    char wadPath[256] = "doom2.wad";  // Default WAD path
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow(window_title);
    
    // Set callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keys);
    glutMouseFunc(mouse);
    
    // Initialize OpenGL
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Load palette
    load_doom_palette();
    
    // Find available WAD files
    find_available_wads();
    
    // If WADs were found, load the first one
    if (num_available_wads > 0) {
        strcpy(wadPath, available_wads[0]);
        load_wad_file(wadPath);
    } else {
        // Try to load the default WAD
        load_wad_file(wadPath);
    }
    
    // Start the main loop
    glutMainLoop();
    return 0;
}

// Find all .WAD files in the current directory
void find_available_wads() {
    WIN32_FIND_DATA findData;
    HANDLE hFind;
    
    num_available_wads = 0;
    
    hFind = FindFirstFile("*.wad", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Skip directories
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                strcpy(available_wads[num_available_wads], findData.cFileName);
                num_available_wads++;
                
                // Limit to 10 WADs
                if (num_available_wads >= 10) {
                    break;
                }
            }
        } while (FindNextFile(hFind, &findData) != 0);
        
        FindClose(hFind);
    }
    
    // If no WADs found, set a default message
    if (num_available_wads == 0) {
        strcpy(status_message, "No WAD files found in the current directory");
    } else {
        sprintf(status_message, "Found %d WAD files. Press L to select a WAD.", num_available_wads);
    }
}

// Clean up the currently loaded WAD resources
void unload_current_wad() {
    if (images) {
        for (int i = 0; i < total_images; i++) {
            if (images[i].data) {
                free(images[i].data);
            }
            if (images[i].texture_id > 0) {
                glDeleteTextures(1, &images[i].texture_id);
            }
        }
        free(images);
        images = NULL;
    }
    
    total_images = 0;
    current_page = 0;
}

void load_doom_palette() {
    FILE *palette_file = fopen("playpal.lmp", "rb");
    
    if (palette_file) {
        // Read the palette from file
        fread(doom_palette, 3, 256, palette_file);
        fclose(palette_file);
        palette_loaded = true;
        sprintf(status_message, "Loaded palette from playpal.lmp");
    } else {
        // If external palette not found, try to extract from WAD
        if (strlen(wad_filename) > 0) {
            if (extract_palette_from_wad(wad_filename)) {
                palette_loaded = true;
                sprintf(status_message, "Extracted palette from %s", wad_filename);
            } else {
                // If all else fails, use a grayscale palette
                for (int i = 0; i < 256; i++) {
                    doom_palette[i][0] = i;
                    doom_palette[i][1] = i;
                    doom_palette[i][2] = i;
                }
                palette_loaded = false;
                sprintf(status_message, "Using grayscale palette (no palette found)");
            }
        }
    }
}

// Extract palette from WAD file
bool extract_palette_from_wad(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    
    // Read WAD header
    wad_header_t header;
    fread(&header, sizeof(wad_header_t), 1, file);
    
    // Check WAD signature
    if (strncmp(header.identifier, "IWAD", 4) != 0 && strncmp(header.identifier, "PWAD", 4) != 0) {
        fclose(file);
        return false;
    }
    
    // Read directory
    wad_directory_t *directory = (wad_directory_t *)malloc(header.num_lumps * sizeof(wad_directory_t));
    fseek(file, header.directory_offset, SEEK_SET);
    fread(directory, sizeof(wad_directory_t), header.num_lumps, file);
    
    bool found_palette = false;
    
    // Look for PLAYPAL lump
    for (int i = 0; i < header.num_lumps; i++) {
        char name[9] = {0};
        strncpy(name, directory[i].name, 8);
        
        if (strcmp(name, "PLAYPAL") == 0) {
            // Go to the beginning of the PLAYPAL lump
            fseek(file, directory[i].file_pos, SEEK_SET);
            
            // PLAYPAL contains multiple palettes (usually 14). We just need the first one.
            fread(doom_palette, 3, 256, file);
            found_palette = true;
            break;
        }
    }
    
    free(directory);
    fclose(file);
    return found_palette;
}

void detect_image_dimensions(wad_image_t *image) {
    // First, check for patch format (standard DOOM sprite format)
    if (image->size >= 8) {
        // First 4 bytes in patch format are header: width (2 bytes) and height (2 bytes)
        int width = (unsigned char)image->data[0] | ((unsigned char)image->data[1] << 8);
        int height = (unsigned char)image->data[2] | ((unsigned char)image->data[3] << 8);
        
        // Sanity check the dimensions
        if (width > 0 && width < 1024 && height > 0 && height < 1024) {
            // Verify column offsets are within bounds
            bool valid_patch = true;
            int *column_offsets = (int*)(image->data + 8);
            
            // Check a few column offsets to see if they make sense
            for (int i = 0; i < width && i < 16; i++) {
                int offset = column_offsets[i];
                if (offset < 0 || offset >= image->size) {
                    valid_patch = false;
                    break;
                }
            }
            
            if (valid_patch) {
                image->width = width;
                image->height = height;
                image->is_valid = true;
                return;
            }
        }
    }
    
    // Check for known fixed sizes (DOOM flats and other fixed-size textures)
    const struct {
        int size;
        int width;
        int height;
        char *type;
    } known_sizes[] = {
        {4096, 64, 64, "flat"},     // Standard floor/ceiling flats
        {16384, 128, 128, "texture"}, // Large textures
        {1024, 32, 32, "texture"},   // Small textures
        {256, 16, 16, "texture"},    // Tiny textures
        {65536, 256, 256, "texture"}, // Very large textures
        {4000, 80, 25, "ENDOOM"},    // ENDOOM screen
        {0, 0, 0, NULL}
    };
    
    for (int i = 0; known_sizes[i].type != NULL; i++) {
        if (image->size == known_sizes[i].size) {
            image->width = known_sizes[i].width;
            image->height = known_sizes[i].height;
            image->is_valid = true;
            return;
        }
    }
    
    // Check if lump name gives us clues about the format
    if (strncmp(image->name, "F_", 2) == 0 ||
        strncmp(image->name, "FLOOR", 5) == 0) {
        // Likely a flat texture (64x64)
        if (image->size % 4096 == 0) {  // Could be multiple flats
            image->width = 64;
            image->height = 64 * (image->size / 4096);
            image->is_valid = true;
            return;
        }
    }
    
    // Try to make a square image from the data
    int side = (int)sqrt((double)image->size);
    if (side * side == image->size) {
        image->width = side;
        image->height = side;
        image->is_valid = true;
        return;
    }
    
    // Try to find a reasonable rectangle
    // First, check power-of-two dimensions (common in graphics)
    int pow2_widths[] = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    for (int i = 0; i < sizeof(pow2_widths)/sizeof(int); i++) {
        int w = pow2_widths[i];
        if (image->size % w == 0) {
            int h = image->size / w;
            // Check if height is also a power of two
            for (int j = 0; j < sizeof(pow2_widths)/sizeof(int); j++) {
                if (h == pow2_widths[j]) {
                    image->width = w;
                    image->height = h;
                    image->is_valid = true;
                    return;
                }
            }
        }
    }
    
    // Try common aspect ratios (ordered by how common they are in DOOM)
    const struct {
        int w;
        int h;
    } ratios[] = {
        {4, 3}, {3, 4}, {1, 1}, {2, 1}, {1, 2}, {16, 9}, {9, 16}, 
        {16, 10}, {10, 16}, {3, 2}, {2, 3}, {5, 4}, {4, 5}
    };
    
    // Try widths up to 1024 pixels (larger than most DOOM textures)
    for (int i = 0; i < sizeof(ratios)/sizeof(ratios[0]); i++) {
        for (int w = 8; w <= 1024; w++) {
            if (image->size % w != 0) continue;
            
            int h = image->size / w;
            
            // Check if these dimensions match our target ratio
            if (w * ratios[i].h == h * ratios[i].w) {
                image->width = w;
                image->height = h;
                image->is_valid = true;
                return;
            }
        }
    }
    
    // Try any width that divides the size evenly
    for (int w = 8; w <= 1024; w++) {
        if (image->size % w == 0) {
            int h = image->size / w;
            // Accept if the aspect ratio isn't too extreme
            if (h > 0 && h < 1024 && w/h <= 8 && h/w <= 8) {
                image->width = w;
                image->height = h;
                image->is_valid = true;
                return;
            }
        }
    }
    
    // Last resort - assign reasonable defaults
    if (image->size > 0) {
        // Choose width based on typical DOOM texture sizes
        if (image->size <= 1024) {
            image->width = 32;  // Small texture
        } else if (image->size <= 4096) {
            image->width = 64;  // Medium/flat texture
        } else {
            image->width = 128; // Large texture
        }
        
        image->height = image->size / image->width;
        if (image->height <= 0) image->height = 1;
        image->is_valid = true;
    } else {
        image->is_valid = false;
    }
}

void create_texture_from_image(wad_image_t *image) {
    if (!image->is_valid || image->size <= 0) return;
    
    // Create RGBA data for texture
    int tex_size = image->width * image->height * 4;
    unsigned char *tex_data = (unsigned char *)malloc(tex_size);
    if (!tex_data) return;
    
    // Clear texture data (transparent black)
    memset(tex_data, 0, tex_size);
    
    // First, check if this is a patch format by examining the header
    bool is_patch = false;
    if (image->size >= 8) {
        int width_header = (unsigned char)image->data[0] | ((unsigned char)image->data[1] << 8);
        int height_header = (unsigned char)image->data[2] | ((unsigned char)image->data[3] << 8);
        
        // If the header width/height match our detected dimensions, it's likely a patch
        if (width_header == image->width && height_header == image->height) {
            is_patch = true;
        }
    }
    
    if (is_patch) {
        // Process DOOM patch format
        int header_size = 8;
        int *column_offsets = (int*)(image->data + header_size);
        
        // Process each column
        for (int x = 0; x < image->width; x++) {
            // Get and validate column offset
            if (x >= image->width) continue; // Bounds check
            
            int offset = column_offsets[x];
            if (offset < 0 || offset >= image->size) continue;
            
            unsigned char *column_ptr = image->data + offset;
            
            // Continue until we hit the 0xFF terminator or exceed image bounds
            while (column_ptr < image->data + image->size) {
                // Get row start position
                int row_start = *column_ptr++;
                if (row_start == 0xFF || column_ptr >= image->data + image->size) break;
                
                // Bounds check
                if (row_start >= image->height) {
                    // Skip invalid span
                    if (column_ptr + 2 >= image->data + image->size) break;
                    int pixel_count = *column_ptr;
                    if (pixel_count < 0 || pixel_count > 255) break; // Sanity check
                    column_ptr += pixel_count + 2; // Skip pixel count + 2 dummy bytes
                    continue;
                }
                
                // Get pixel count
                if (column_ptr >= image->data + image->size) break;
                int pixel_count = *column_ptr++;
                if (pixel_count < 0 || pixel_count > 255 || column_ptr >= image->data + image->size) break;
                
                // Bounds check for pixel count
                if (row_start + pixel_count > image->height) {
                    pixel_count = image->height - row_start;
                    if (pixel_count <= 0) continue;
                }
                
                // Skip dummy byte
                column_ptr++;
                if (column_ptr >= image->data + image->size) break;
                
                // Process pixel data
                for (int y = 0; y < pixel_count; y++) {
                    if (column_ptr >= image->data + image->size) break;
                    
                    unsigned char pixel_index = *column_ptr++;
                    
                    // Check if index is valid for the palette
                    if (pixel_index < 256) {
                        // Get palette color
                        unsigned char r = doom_palette[pixel_index][0];
                        unsigned char g = doom_palette[pixel_index][1];
                        unsigned char b = doom_palette[pixel_index][2];
                        
                        // DOOM uses index 255 for transparent in some cases
                        unsigned char a = (pixel_index == 255) ? 0 : 255;
                        
                        // Set RGBA in destination
                        int dest_idx = ((row_start + y) * image->width + x) * 4;
                        if (dest_idx + 3 < tex_size) {
                            tex_data[dest_idx + 0] = r;
                            tex_data[dest_idx + 1] = g;
                            tex_data[dest_idx + 2] = b;
                            tex_data[dest_idx + 3] = a;
                        }
                    }
                }
                
                // Skip dummy byte at end of column
                if (column_ptr >= image->data + image->size) break;
                column_ptr++;
            }
        }
    } else {
        // Determine if this is likely a flat based on name prefix or size
        bool is_flat = ((image->size == 4096 && image->width == 64 && image->height == 64) ||
                        strncmp(image->name, "F_", 2) == 0 ||
                        strncmp(image->name, "FLAT", 4) == 0 ||
                        strncmp(image->name, "FLOOR", 5) == 0 ||
                        strncmp(image->name, "CEIL", 4) == 0);
        
        // Handle as raw pixel data (common for flats, colormaps, etc.)
        for (int y = 0; y < image->height; y++) {
            for (int x = 0; x < image->width; x++) {
                // Calculate source index
                int src_idx = y * image->width + x;
                
                // Skip if out of bounds
                if (src_idx >= image->size) continue;
                
                // Get palette index
                unsigned char pixel_index = image->data[src_idx];
                
                // Get palette color
                unsigned char r = doom_palette[pixel_index][0];
                unsigned char g = doom_palette[pixel_index][1];
                unsigned char b = doom_palette[pixel_index][2];
                
                // Special handling for transparency:
                // - For flats, everything is opaque
                // - For most other textures, index 0 is often transparent
                // - Some textures use 255 as transparent instead
                unsigned char a = 255;
                if (!is_flat) {
                    if (pixel_index == 0 || pixel_index == 255) {
                        // Detect if this might be a solid texture vs a sprite
                        // In a sprite, there are typically many transparent pixels
                        // We'll need to load the entire texture first to determine this
                        a = 0;
                    }
                }
                
                // Set RGBA in destination
                int dest_idx = (y * image->width + x) * 4;
                if (dest_idx + 3 < tex_size) {
                    tex_data[dest_idx + 0] = r;
                    tex_data[dest_idx + 1] = g;
                    tex_data[dest_idx + 2] = b;
                    tex_data[dest_idx + 3] = a;
                }
            }
        }
        
        // Post-process for raw pixels: if most pixels are transparent, this is likely
        // not a correct interpretation. Make everything opaque in that case.
        if (!is_flat) {
            int transparent_count = 0;
            for (int i = 0; i < image->width * image->height; i++) {
                if (tex_data[i * 4 + 3] == 0) {
                    transparent_count++;
                }
            }
            
            // If more than 90% transparent, make everything opaque
            if (transparent_count > image->width * image->height * 0.9) {
                for (int i = 0; i < image->width * image->height; i++) {
                    tex_data[i * 4 + 3] = 255; // Make opaque
                }
            }
        }
    }
    
    // Generate OpenGL texture
    glGenTextures(1, &image->texture_id);
    glBindTexture(GL_TEXTURE_2D, image->texture_id);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // Create the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
    
    free(tex_data);
}

bool is_image_lump(char *name) {
    // Remove trailing spaces from name
    char clean_name[9] = {0};
    strncpy(clean_name, name, 8);
    int len = strlen(clean_name);
    while (len > 0 && clean_name[len-1] == ' ') {
        clean_name[len-1] = '\0';
        len--;
    }
    
    // Known NON-image lumps to skip
    
    const char *skip_lumps[] = {
    /*  "PLAYPAL", "COLORMAP", "ENDOOM", "DEMO", "DS", "PC", "D_",
        "THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
        "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP",
        "BEHAVIOR", "SCRIPTS", "GENMIDI", "DMXGUS", 
        "DEHACKED", "MAPINFO", "SNDINFO", "SNDSEQ", "CONVERSATION",
        "ANIMDEFS", "LANGUAGE", "KEYCONF", "FONTDEFS",
        "TERRAIN", "SBARINFO", "LOCKDEFS",
        "TITLEMAP", "TEXTMAP", "DIALOGUE", "ZNODES", "EXTENDED",
        "LOADACS",  "ANIMATED", "README", "INFO", "LOG" */
    };
    
    // Check if lump is in skip list
    for (int i = 0; i < sizeof(skip_lumps)/sizeof(char*); i++) {
        if (strncmp(clean_name, skip_lumps[i], strlen(skip_lumps[i])) == 0) {
            return false;
        }
    }
    
    // Skip MAP markers (MAPxx, ExMy)
    if ((len == 5 && strncmp(clean_name, "MAP", 3) == 0 && 
         isdigit(clean_name[3]) && isdigit(clean_name[4])) ||
        (len == 4 && clean_name[0] == 'E' && isdigit(clean_name[1]) && 
         clean_name[2] == 'M' && isdigit(clean_name[3]))) {
        return false;
    }
    
    // Check for known graphic lumps by prefix
    const char *graphic_prefixes[] = {
        "WALL", "DOOR", "FLOOR", "CEIL", "SKY", "STEP", "GATE",
        "F_", "S_", "F", "P", "P1", "P2", "P3", "SP", "T", "DP",
        "SW", "M_", "ST", "WI", "BRDR", "PLAYA", "PLAYB", "PUNCH",
        "PISTA", "PISTB", "PISFA", "PISFB", "PIATA", "PIATB", "SGUNA",
        "SGUNB", "SHTFA", "SHTFB", "MGUNA", "MGUNB", "LAUNCA", "LAUNCB",
        "PLASA", "PLASB", "BFUGA", "BFUGB", "SAWGA", "SAWGB", "MISFA",
        "MISFB", "AMMOA", "AMMOB", "MEDIA", "MEDIB", "STIMA", "STIMB",
        "CELLA", "CELLB", "ARM1A", "ARM1B", "ARM2A", "ARM2B", "STARA",
        "STARB", "KEYA", "KEYB", "BKEYA", "BKEYB", "RKEYA", "RKEYB",
        "YKEYA", "YKEYB", "BSKUA", "BSKUB", "RSKUA", "RSKUB", "YSKUA",
        "YSKUB", "BPAKA", "BPAKB", "RPAKA", "RPAKB", "GPAKA", "GPAKB",
        "PIN", "MEGA", "SHT", "PUNG", "PIST", "SHOT", "MGUN", "ROCK",
        "PLSM", "BFGG", "SAW", "CSA", "CLP", "STIM", "MEDI", "SOUL",
        "BON", "BON1", "BON2", "PMAP", "PINV", "PVIS", "ARM", "ARM1",
        "ARM2", "BAR", "CEYE", "FCAN", "TLP", "TNT1", "GIB", "ELEC",
        "POL", "POB", "BERY", "BLD", "FIRE", "WATR", "SLME", "POL5",
        "BRS1", "PUF", "PUF1", "PUF2", "PUF3", "PUF4", "TRE1", "TRE2",
        "BAL1", "BAL2", "BAL7", "BFS1", "BFE1", "BFE2", "MISL", "PLASMA"
    };
    
    for (int i = 0; i < sizeof(graphic_prefixes)/sizeof(char*); i++) {
        if (strncmp(clean_name, graphic_prefixes[i], strlen(graphic_prefixes[i])) == 0) {
            return true;
        }
    }
    
    // Known graphic lumps by full name
    const char *graphic_names[] = {
        "TITLEPIC", "INTERPIC", "BOSSBACK", "PFUB1", "PFUB2",
        "HELP", "HELP1", "HELP2", "CREDIT", "VICTORY", "VICTORY2",
        "FINAL", "END", "STBAR", "BACK", "RSKY1", "RSKY2", "RSKY3",
        "LOGO", "PFUB1", "PFUB2", "WINUM0", "WINUM1", "WINUM2", "WINUM3",
        "WINUM4", "WINUM5", "WINUM6", "WINUM7", "WINUM8", "WINUM9",
        "WIURH0", "WIURH1", "WISPLAT", "WIKILRS", "WIBP1", "WIBP2",
        "WIBP3", "WIBP4", "STFST", "STFTR", "STFTL", "STFOUCH",
        "STFEVL", "STFKILL", "STTMINUS", "STTPRCNT", "STYSNUM0",
        "LOADING", "TITLE", "BKGND", "PANEL", "PAUSE", "OPTION"
    };
    
    for (int i = 0; i < sizeof(graphic_names)/sizeof(char*); i++) {
        if (strcmp(clean_name, graphic_names[i]) == 0) {
            return true;
        }
    }
    
    // Check for flats (usually 64x64 textures with F_prefix)
    if (strncmp(clean_name, "F_", 2) == 0 || 
        strncmp(clean_name, "FLAT", 4) == 0 ||
        strncmp(clean_name, "FLOOR", 5) == 0 ||
        strncmp(clean_name, "CEIL", 4) == 0) {
        return true;
    }
    
    // Sprite naming patterns
    // First letter is usually the sprite type code like "TROO" for Imp
    // Followed by A-Z for animation frame and 0-9 for rotation angle
    if (len >= 5 && 
        isalpha(clean_name[0]) && isalpha(clean_name[1]) && 
        isalpha(clean_name[2]) && isalpha(clean_name[3]) &&
        isalpha(clean_name[4])) {
        return true;
    }
    
    // 4+2 pattern (4 letters + frame + angle)
    if (len >= 6 && 
        isalpha(clean_name[0]) && isalpha(clean_name[1]) && 
        isalpha(clean_name[2]) && isalpha(clean_name[3]) &&
        isalpha(clean_name[4]) && isdigit(clean_name[5])) {
        return true;
    }
    
    // Additional heuristic: consider anything that's exactly 4096 bytes as a flat
    // for (int i = 0; i < header.num_lumps; i++) {
    //     if (directory[i].size == 4096 && strcmp(directory[i].name, clean_name) == 0) {
    //         return true;
    //     }
    // }
    
    // Consider anything with plausible graphic size (not too small, not too large)
    // This will be checked later when we determine dimensions
    if (len >= 3 && !isdigit(clean_name[0])) { // Basic filter to avoid pure numbers
        return true;
    }
    
    return false;
}

void load_wad_file(const char *filename) {
    // First unload any currently loaded WAD
    unload_current_wad();
    
    FILE *file = fopen(filename, "rb");
    if (!file) {
        sprintf(status_message, "Error: Cannot open file %s", filename);
        return;
    }
    
    // Store the filename
    strcpy(wad_filename, filename);
    
    // Read WAD header
    wad_header_t header;
    fread(&header, sizeof(wad_header_t), 1, file);
    
    // Check WAD signature
    if (strncmp(header.identifier, "IWAD", 4) != 0 && strncmp(header.identifier, "PWAD", 4) != 0) {
        sprintf(status_message, "Error: %s is not a valid WAD file", filename);
        fclose(file);
        return;
    }
    
    // Update window title with WAD info
    sprintf(window_title, "DOOM WAD Image Viewer - %s (%s, %d lumps)", 
            filename, header.identifier, header.num_lumps);
    glutSetWindowTitle(window_title);

    // Load palette first - try to extract from this WAD
    if (!palette_loaded || (palette_loaded && !strcmp(status_message, "Using grayscale palette (no palette found)"))) {
        if (extract_palette_from_wad(filename)) {
            palette_loaded = true;
            strcpy(status_message, "Extracted palette from WAD file");
        } else {
            load_doom_palette();  // Try to load from external file
        }
    }
    
    // Read directory
    wad_directory_t *directory = (wad_directory_t *)malloc(header.num_lumps * sizeof(wad_directory_t));
    fseek(file, header.directory_offset, SEEK_SET);
    fread(directory, sizeof(wad_directory_t), header.num_lumps, file);
    
    // First pass: count images
    total_images = 0;
    for (int i = 0; i < header.num_lumps; i++) {
        // Make sure name is null-terminated
        char name[9] = {0};
        strncpy(name, directory[i].name, 8);
        
        if (is_image_lump(name) && directory[i].size > 0) {
            total_images++;
        }
    }
    
    // Allocate image array
    images = (wad_image_t *)malloc(total_images * sizeof(wad_image_t));
    if (!images) {
        sprintf(status_message, "Error: Memory allocation failed");
        free(directory);
        fclose(file);
        return;
    }
    
    // Second pass: load images
    int image_index = 0;
    for (int i = 0; i < header.num_lumps; i++) {
        char name[9] = {0};
        strncpy(name, directory[i].name, 8);
        
        if (is_image_lump(name) && directory[i].size > 0) {
            // Copy lump name
            strcpy(images[image_index].name, name);
            
            // Allocate memory for image data
            images[image_index].data = (unsigned char *)malloc(directory[i].size);
            if (!images[image_index].data) continue;
            
            // Read image data
            fseek(file, directory[i].file_pos, SEEK_SET);
            fread(images[image_index].data, directory[i].size, 1, file);
            
            // Set image properties
            images[image_index].size = directory[i].size;
            
            // Try to determine image dimensions
            detect_image_dimensions(&images[image_index]);
            
            // Create OpenGL texture for this image
            if (images[image_index].is_valid) {
                create_texture_from_image(&images[image_index]);
            }
            
            image_index++;
        }
    }
    
    // Update status message
    if (!palette_loaded) {
        sprintf(status_message, "Loaded %d images from %s (using grayscale - no palette found)", 
                total_images, filename);
    } else {
        sprintf(status_message, "Loaded %d images from %s with color palette", 
                total_images, filename);
    }
    
    // Cleanup
    free(directory);
    fclose(file);
}

void draw_string(float x, float y, const char *text) {
    glRasterPos2f(x, y);
    for (const char *c = text; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
    }
}

// The file selector menu
void file_selector_menu() {
    // Semi-transparent background
    glColor4f(0.0, 0.0, 0.5, 0.8);
    glEnable(GL_BLEND);
    glBegin(GL_QUADS);
        glVertex2f(window_width/4, window_height/4);
        glVertex2f(window_width*3/4, window_height/4);
        glVertex2f(window_width*3/4, window_height*3/4);
        glVertex2f(window_width/4, window_height*3/4);
    glEnd();
    glDisable(GL_BLEND);
    
    // Title
    glColor3f(1.0, 1.0, 0.0);
    draw_string(window_width/4 + 20, window_height/4 + 20, "Select a WAD file to load:");
    
    // List of WAD files
    for (int i = 0; i < num_available_wads; i++) {
        if (i == selected_wad_index) {
            // Highlight selected item
            glColor3f(1.0, 1.0, 0.0);
            glBegin(GL_QUADS);
                glVertex2f(window_width/4 + 10, window_height/4 + 40 + i*20 - 3);
                glVertex2f(window_width*3/4 - 10, window_height/4 + 40 + i*20 - 3);
                glVertex2f(window_width*3/4 - 10, window_height/4 + 40 + i*20 + 15);
                glVertex2f(window_width/4 + 10, window_height/4 + 40 + i*20 + 15);
            glEnd();
            glColor3f(0.0, 0.0, 0.0);
        } else {
            glColor3f(1.0, 1.0, 1.0);
        }
        
        draw_string(window_width/4 + 20, window_height/4 + 40 + i*20, available_wads[i]);
    }
    
    // Instructions
    glColor3f(0.7, 0.7, 1.0);
    draw_string(window_width/4 + 20, window_height*3/4 - 40, "Use Up/Down to select, Enter to load, Esc to cancel");
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Set up 2D projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, window_width, window_height, 0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Calculate how many images we can display per page
    images_per_page = images_per_row * ((window_height - 50) / (image_size + image_padding));
    if (images_per_page <= 0) images_per_page = 1;
    
    // Calculate total pages
    int total_pages = 1;
    if (total_images > 0) {
        total_pages = (total_images + images_per_page - 1) / images_per_page;
    }
    
    // Make sure current page is valid
    if (current_page >= total_pages) current_page = total_pages - 1;
    if (current_page < 0) current_page = 0;
    
    // Calculate starting image index
    int start_idx = current_page * images_per_page;
    
    // Draw images for current page
    for (int i = 0; i < images_per_page && start_idx + i < total_images; i++) {
        int row = i / images_per_row;
        int col = i % images_per_row;
        
        // Calculate position but ensure integer coordinates
        int x = col * (image_size + image_padding) + image_padding;
        int y = row * (image_size + image_padding) + image_padding + 30; // 30px for header
        
        wad_image_t *img = &images[start_idx + i];
        
        if (img->is_valid && img->texture_id > 0) {
            // Calculate aspect ratio
            float aspect_ratio = (float)img->width / (float)img->height;
            
            // Calculate display dimensions while preserving aspect ratio
            int display_width, display_height;
            
            if (aspect_ratio >= 1.0) {
                // Wider than tall
                display_width = image_size;
                display_height = (int)(image_size / aspect_ratio);
            } else {
                // Taller than wide
                display_height = image_size;
                display_width = (int)(image_size * aspect_ratio);
            }
            
            // Center the image in its allocated space - ensure integer coordinates
            int x_offset = (image_size - display_width) / 2;
            int y_offset = (image_size - display_height) / 2;
            
            // Draw image - use integer coordinates to ensure pixel-perfect alignment
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, img->texture_id);
            
            // Force glTexParameteri to enforce nearest-neighbor interpolation
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            
            glColor3f(1.0, 1.0, 1.0);
            glBegin(GL_QUADS);
                glTexCoord2f(0.0, 0.0); glVertex2i(x + x_offset, y + y_offset);
                glTexCoord2f(1.0, 0.0); glVertex2i(x + x_offset + display_width, y + y_offset);
                glTexCoord2f(1.0, 1.0); glVertex2i(x + x_offset + display_width, y + y_offset + display_height);
                glTexCoord2f(0.0, 1.0); glVertex2i(x + x_offset, y + y_offset + display_height);
            glEnd();
            
            glDisable(GL_TEXTURE_2D);
            
            // Draw image name
            char label[64];
            sprintf(label, "%s (%dx%d)", img->name, img->width, img->height);
            
            glColor3f(1.0, 1.0, 0.0);
            draw_string(x, y + image_size + 12, label);
        } else {
            // Draw placeholder for invalid image
            glColor3f(0.5, 0.5, 0.5);
            glBegin(GL_QUADS);
                glVertex2i(x, y);
                glVertex2i(x + image_size, y);
                glVertex2i(x + image_size, y + image_size);
                glVertex2i(x, y + image_size);
            glEnd();
            
            glColor3f(1.0, 0.0, 0.0);
            draw_string(x, y + image_size / 2, "Invalid Image");
            draw_string(x, y + image_size / 2 + 15, img->name);
        }
    }
    
    // Draw status bar
    glColor3f(0.0, 0.0, 0.0);
    glBegin(GL_QUADS);
        glVertex2i(0, window_height - 20);
        glVertex2i(window_width, window_height - 20);
        glVertex2i(window_width, window_height);
        glVertex2i(0, window_height);
    glEnd();
    
    // Draw page info
    char page_info[64];
    sprintf(page_info, "Page %d/%d - %d images total", 
            current_page + 1, total_pages, total_images);
    
    glColor3f(1.0, 1.0, 1.0);
    draw_string(10, window_height - 5, status_message);
    draw_string(window_width - 200, window_height - 5, page_info);
    
    // Draw header
    glColor3f(0.0, 0.0, 0.0);
    glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(window_width, 0);
        glVertex2i(window_width, 25);
        glVertex2i(0, 25);
    glEnd();
    
    glColor3f(1.0, 1.0, 1.0);
    char header_text[256];
    sprintf(header_text, "DOOM WAD Image Viewer - %s - Press L to load a different WAD, H for help", 
            strlen(wad_filename) > 0 ? wad_filename : "No WAD loaded");
    draw_string(10, 15, header_text);
    
    // Show help screen if requested
    if (show_help) {
        // Semi-transparent background
        glColor4f(0.0, 0.0, 0.5, 0.8);
        glEnable(GL_BLEND);
        glBegin(GL_QUADS);
            glVertex2i(window_width/4, window_height/4);
            glVertex2i(window_width*3/4, window_height/4);
            glVertex2i(window_width*3/4, window_height*3/4);
            glVertex2i(window_width/4, window_height*3/4);
        glEnd();
        glDisable(GL_BLEND);
        
        // Help text
        glColor3f(1.0, 1.0, 1.0);
        const char *help_text[] = {
            "DOOM WAD Image Viewer - Help",
            "",
            "Navigation:",
            "  Page Up/Down - Move between pages",
            "  Home/End - Go to first/last page",
            "",
            "Display Options:",
            "  +/- - Change image size",
            "  Left/Right - Change images per row",
            "",
            "Other Controls:",
            "  L - Load a different WAD file",
            "  R - Refresh available WAD files",
            "  H - Toggle help screen",
            "  8 - Convert PNG folder to WAD file",
            "  Esc - Quit program",
            "",
            "Press any key to close this help"
        };
        
        int y_pos = window_height/4 + 20;
        for (int i = 0; i < 17; i++) {
            draw_string(window_width/4 + 20, y_pos, help_text[i]);
            y_pos += 20;
        }
    }
    
    // Show file selector if requested
    if (show_file_selector) {
        file_selector_menu();
    }
    // show folder selector if
    if (show_folder_selector) {
        folder_selector_menu();
    }
    
    glutSwapBuffers();
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    
    // Recalculate images per page
    images_per_page = images_per_row * ((window_height - 50) / (image_size + image_padding));
    if (images_per_page <= 0) images_per_page = 1;
}

void keyboard(unsigned char key, int x, int y) {
    if (show_file_selector) {
        switch (key) {
            case 27: // Esc
                show_file_selector = false;
                break;
                
            case 13: // Enter
                if (num_available_wads > 0) {
                    load_wad_file(available_wads[selected_wad_index]);
                    show_file_selector = false;
                }
                break;
        }
    } else if (show_help) {
        // Any key closes help
        show_help = false;
    } else {
        switch (key) {
            case 27: // Esc key
                exit(0);
                break;
                
            case 'h':
            case 'H':
                show_help = true;
                break;

                case '8':
                case '*':
                    show_folder_selector = true;
                    selected_input_field = 0;  // Reset to first input field
                    strcpy(input_png_folder, "");  // Clear previous inputs
                    strcpy(wad_output_name, "");
                    strcpy(output_wad_folder, "");
                    break;
                
            case 'l':
            case 'L':
                show_file_selector = true;
                selected_wad_index = 0;  // Reset selection
                break;
                
            case 'r':
            case 'R':
                find_available_wads();
                break;
                
            case '+':
            case '=':
                image_size += 16;
                if (image_size > 256) image_size = 256;
                break;
                
            case '-':
            case '_':
                image_size -= 16;
                if (image_size < 32) image_size = 32;
                break;
        }
    }

    if (show_folder_selector) {
        switch(key) {
            case 9:  // Tab key
                selected_input_field = (selected_input_field + 1) % 3;
                break;
            
            case 27:  // Escape key
                show_folder_selector = false;
                break;
            
            case 13:  // Enter key
                // Validate and perform conversion
                if (strlen(input_png_folder) > 0 && 
                    strlen(wad_output_name) > 0 && 
                    strlen(output_wad_folder) > 0) {
                    int result = bmp32_to_pwad(input_png_folder, wad_output_name, output_wad_folder);
                    if (result) {
                        sprintf(status_message, "Successfully converted PNGs to %s/%s.wad", 
                                output_wad_folder, wad_output_name);
                    } else {
                        sprintf(status_message, "Failed to convert PNGs");
                    }
                    show_folder_selector = false;
                }
                break;
            
            case 8:  // Backspace key
                switch(selected_input_field) {
                    case 0:  // Input PNG Folder
                        if (strlen(input_png_folder) > 0)
                            input_png_folder[strlen(input_png_folder)-1] = '\0';
                        break;
                    case 1:  // WAD Output Name
                        if (strlen(wad_output_name) > 0)
                            wad_output_name[strlen(wad_output_name)-1] = '\0';
                        break;
                    case 2:  // Output WAD Folder
                        if (strlen(output_wad_folder) > 0)
                            output_wad_folder[strlen(output_wad_folder)-1] = '\0';
                        break;
                }
                break;
            
            default:
                // Prevent input of very long paths
                switch(selected_input_field) {
                    case 0:  // Input PNG Folder
                        if (strlen(input_png_folder) < sizeof(input_png_folder) - 1 && 
                            (isalnum(key) || key == '/' || key == '.' || key == '_' || key == '-' || key == ' ')) {
                            input_png_folder[strlen(input_png_folder)] = key;
                            input_png_folder[strlen(input_png_folder)+1] = '\0';
                        }
                        break;
                    case 1:  // WAD Output Name
                        if (strlen(wad_output_name) < sizeof(wad_output_name) - 1 && 
                            (isalnum(key) || key == '_')) {
                            wad_output_name[strlen(wad_output_name)] = key;
                            wad_output_name[strlen(wad_output_name)+1] = '\0';
                        }
                        break;
                    case 2:  // Output WAD Folder
                        if (strlen(output_wad_folder) < sizeof(output_wad_folder) - 1 && 
                            (isalnum(key) || key == '/' || key == '.' || key == '_' || key == '-' || key == ' ')) {
                            output_wad_folder[strlen(output_wad_folder)] = key;
                            output_wad_folder[strlen(output_wad_folder)+1] = '\0';
                        }
                        break;
                }
                break;
        }
        glutPostRedisplay();
        return;
    }
    
    glutPostRedisplay();
}

void special_keys(int key, int x, int y) {
    if (show_file_selector) {
        switch (key) {
            case GLUT_KEY_UP:
                selected_wad_index--;
                if (selected_wad_index < 0) {
                    selected_wad_index = num_available_wads - 1;
                }
                break;
                
            case GLUT_KEY_DOWN:
                selected_wad_index++;
                if (selected_wad_index >= num_available_wads) {
                    selected_wad_index = 0;
                }
                break;
        }
    } else {
        switch (key) {
            case GLUT_KEY_PAGE_UP:
                current_page--;
                if (current_page < 0) current_page = 0;
                break;
                
            case GLUT_KEY_PAGE_DOWN:
                current_page++;
                // Upper bound check happens in display()
                break;
                
            case GLUT_KEY_HOME:
                current_page = 0;
                break;
                
            case GLUT_KEY_END:
                // Set to a high value, will be clamped in display()
                current_page = 9999;
                break;
                
            case GLUT_KEY_LEFT:
                images_per_row--;
                if (images_per_row < 1) images_per_row = 1;
                break;
                
            case GLUT_KEY_RIGHT:
                images_per_row++;
                if (images_per_row > 8) images_per_row = 8;
                break;
        }
    }
    
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Calculate which image was clicked (if any)
        int row = (y - 30) / (image_size + image_padding);
        int col = x / (image_size + image_padding);
        
        if (row >= 0 && col >= 0 && col < images_per_row) {
            int idx = current_page * images_per_page + row * images_per_row + col;
            if (idx >= 0 && idx < total_images) {
                // Display info about the clicked image
                wad_image_t *img = &images[idx];
                sprintf(status_message, "Selected: %s (%dx%d, %d bytes)", 
                        img->name, img->width, img->height, img->size);
            }
        }
    }
    
    glutPostRedisplay();
}


// BMP32 header structures
typedef struct {
    uint16_t file_type;     // Must be 0x4D42 ('BM')
    uint32_t file_size;     // Size of the file in bytes
    uint16_t reserved1;     // Reserved, must be zero
    uint16_t reserved2;     // Reserved, must be zero
    uint32_t offset_data;   // Offset to image data in bytes
} BMP_FILE_HEADER;

typedef struct {
    uint32_t header_size;   // Size of this header
    int32_t width;          // Image width
    int32_t height;         // Image height
    uint16_t planes;        // Must be 1
    uint16_t bit_count;     // Bits per pixel
    uint32_t compression;   // Compression method
    uint32_t image_size;    // Size of image data
    int32_t x_pixels_per_m; // Pixels per meter in X
    int32_t y_pixels_per_m; // Pixels per meter in Y
    uint32_t colors_used;   // Number of colors in color palette
    uint32_t colors_important; // Number of important colors
} BMP_INFO_HEADER;

int validate_bmp32_header(FILE* file, BMP_FILE_HEADER* file_header, BMP_INFO_HEADER* info_header) {
    // Read file header
    if (fread(file_header, sizeof(BMP_FILE_HEADER), 1, file) != 1) {
        fprintf(stderr, "Error reading BMP file header\n");
        return 0;
    }

    // Validate file type (must be 'BM')
    if (file_header->file_type != 0x4D42) {
        fprintf(stderr, "Invalid BMP file type\n");
        return 0;
    }

    // Read info header
    if (fread(info_header, sizeof(BMP_INFO_HEADER), 1, file) != 1) {
        fprintf(stderr, "Error reading BMP info header\n");
        return 0;
    }

    // Detailed diagnostic print
    printf("File Header Diagnostics:\n");
    printf("  File Type: 0x%04X\n", file_header->file_type);
    printf("  File Size: %u bytes\n", file_header->file_size);
    printf("  Offset to Data: %u\n", file_header->offset_data);

    printf("Info Header Diagnostics:\n");
    printf("  Header Size: %u\n", info_header->header_size);
    printf("  Width: %d\n", info_header->width);
    printf("  Height: %d\n", info_header->height);
    printf("  Planes: %u\n", info_header->planes);
    printf("  Bit Count: %u\n", info_header->bit_count);
    printf("  Compression: %u\n", info_header->compression);

    // Validate image attributes more carefully
    if (info_header->planes != 1) {
        fprintf(stderr, "Invalid number of planes: %u (must be 1)\n", info_header->planes);
        return 0;
    }

    // More flexible bit depth check
    if (info_header->bit_count != 24 && info_header->bit_count != 32) {
        fprintf(stderr, "Unsupported bit depth: %u (expected 24 or 32)\n", info_header->bit_count);
        return 0;
    }

    // Ensure sane image dimensions
    if (info_header->width <= 0 || info_header->width > 512 ||
        info_header->height <= 0 || info_header->height > 512) {
        fprintf(stderr, "Invalid image dimensions: %dx%d\n", 
                info_header->width, info_header->height);
        return 0;
    }

    return 1;
}

// Modify conversion function to handle 24-bit BMPs
int bmp32_to_doom_patch_optimized(const char* input_bmp, unsigned char** output_data, int* output_size) {
    FILE* file = fopen(input_bmp, "rb");
    if (!file) {
        fprintf(stderr, "Could not open BMP file: %s\n", input_bmp);
        return 0;
    }

    BMP_FILE_HEADER file_header;
    BMP_INFO_HEADER info_header;

    // Validate headers
    if (!validate_bmp32_header(file, &file_header, &info_header)) {
        fclose(file);
        return 0;
    }

    // Correct width and height handling for BMP
    int width = abs(info_header.width);
    int height = abs(info_header.height);
    int bytes_per_pixel = (info_header.bit_count == 32) ? 4 : 3;

    // Debug print
    printf("Processing image: %s, Dimensions: %dx%d, Bit Depth: %d\n", 
           input_bmp, width, height, info_header.bit_count);

    // Allocate robust output buffer
    unsigned char* patch_data = malloc(width * height * 4 + 1024);
    int current_offset = 0;

    // Write DOOM patch header (width and height)
    memcpy(patch_data + current_offset, &width, 2);
    current_offset += 2;
    memcpy(patch_data + current_offset, &height, 2);
    current_offset += 2;
    
    // Allocate column offset table
    int* column_offsets = malloc(width * sizeof(int));
    memset(patch_data + current_offset, 0, width * 4);
    current_offset += width * 4;

    // Seek to pixel data
    fseek(file, file_header.offset_data, SEEK_SET);

    // Allocate pixel buffer with padding
    int row_size = ((width * info_header.bit_count + 31) / 32) * 4;  // 32-bit aligned row size
    unsigned char* pixel_data = malloc(row_size * height);
    memset(pixel_data, 0, row_size * height);
    
    // Read pixel data (bottom-up for BMP)
    for (int y = height - 1; y >= 0; y--) {
        if (fread(pixel_data + y * row_size, 1, width * bytes_per_pixel, file) != width * bytes_per_pixel) {
            fprintf(stderr, "Failed to read pixel data for row %d\n", y);
            free(patch_data);
            free(column_offsets);
            free(pixel_data);
            fclose(file);
            return 0;
        }
    }

    // Process each column for DOOM patch format
    for (int x = 0; x < width; x++) {
        column_offsets[x] = current_offset;
        
        int span_start = -1;
        unsigned char span_pixels[256];
        int span_count = 0;

        for (int y = 0; y < height; y++) {
            int pixel_offset = (y * row_size) + (x * bytes_per_pixel);
            unsigned char* pixel = pixel_data + pixel_offset;
            
            // Check alpha/transparency for 32-bit, or always visible for 24-bit
            int is_visible = 1;
            if (bytes_per_pixel == 4) {
                is_visible = pixel[3] >= 128;
            }
            
            // Transparent check
            if (!is_visible) {
                // End current span if one was in progress
                if (span_start != -1) {
                    // Write span details
                    patch_data[current_offset++] = span_start;
                    patch_data[current_offset++] = span_count;
                    patch_data[current_offset++] = 0; // Dummy before pixels
                    
                    memcpy(patch_data + current_offset, span_pixels, span_count);
                    current_offset += span_count;
                    
                    patch_data[current_offset++] = 0; // Dummy after pixels
                    
                    // Reset span
                    span_start = -1;
                    span_count = 0;
                }
                continue;
            }

            // Start or continue span
            if (span_start == -1) {
                span_start = y;
            }

            // Convert RGB to DOOM palette index (different handling for 24 vs 32-bit)
            if (bytes_per_pixel == 4) {
                span_pixels[span_count++] = 
                    ((pixel[2] >> 2) + 
                     (pixel[1] >> 2) * 4 + 
                     (pixel[0] >> 2) * 16);
            } else {
                span_pixels[span_count++] = 
                    ((pixel[2] >> 2) + 
                     (pixel[1] >> 2) * 4 + 
                     (pixel[0] >> 2) * 16);
            }
        }

        // Handle final span
        if (span_start != -1) {
            patch_data[current_offset++] = span_start;
            patch_data[current_offset++] = span_count;
            patch_data[current_offset++] = 0;
            
            memcpy(patch_data + current_offset, span_pixels, span_count);
            current_offset += span_count;
            
            patch_data[current_offset++] = 0;
        }

        // Column terminator
        patch_data[current_offset++] = 0xFF;
    }

    // Update column offsets
    memcpy(patch_data + 8, column_offsets, width * 4);

    // Cleanup
    free(column_offsets);
    free(pixel_data);

    // Set output
    *output_data = patch_data;
    *output_size = current_offset;

    fclose(file);

    return 1;
}

// BMP to PWAD conversion function (similar to pngtopwad)
int bmp32_to_pwad(const char* input_folder, const char* wad_name, const char* output_folder) {
    DIR* dir;
    struct dirent* entry;
    char input_path[1024];
    char output_path[1024];
    FILE* pwad_file;
    int bmp_count = 0;
    
    // Open input directory
    dir = opendir(input_folder);
    if (!dir) {
        fprintf(stderr, "Error opening input folder: %s\n", strerror(errno));
        return 0;
    }
    
    // First pass: count valid BMP files
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".bmp") != NULL) {
            bmp_count++;
        }
    }
    rewinddir(dir);
    
    // Prepare PWAD output file
    snprintf(output_path, sizeof(output_path), "%s/%s.wad", output_folder, wad_name);
    pwad_file = fopen(output_path, "wb");
    if (!pwad_file) {
        fprintf(stderr, "Error creating WAD file: %s\n", strerror(errno));
        closedir(dir);
        return 0;
    }
    
    // PWAD header
    PWADHeader header;
    strncpy(header.magic, "PWAD", 4);
    header.num_lumps = bmp_count;
    header.directory_pos = sizeof(PWADHeader);
    
    // Write placeholder header
    fwrite(&header, sizeof(PWADHeader), 1, pwad_file);
    
    // Allocate lump entries
    LumpEntry* lumps = malloc(bmp_count * sizeof(LumpEntry));
    if (!lumps) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(pwad_file);
        closedir(dir);
        return 0;
    }
    
    // Second pass: process each BMP
    int lump_index = 0;
    int current_pos = sizeof(PWADHeader);
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".bmp") != NULL) {
            // Construct full input path
            snprintf(input_path, sizeof(input_path), "%s/%s", input_folder, entry->d_name);
            
            // Convert BMP to DOOM patch
            unsigned char* patch_data = NULL;
            int patch_size = 0;
            if (!bmp32_to_doom_patch_optimized(input_path, &patch_data, &patch_size)) {
                fprintf(stderr, "Failed to convert %s\n", input_path);
                continue;
            }
            
            // Write patch data to WAD
            fwrite(patch_data, 1, patch_size, pwad_file);
            
            // Prepare lump entry
            char lump_name[9] = {0};
            strncpy(lump_name, entry->d_name, sizeof(lump_name) - 1);
            char *dot = strrchr(lump_name, '.');
            if (dot) *dot = '\0';
            
            strncpy(lumps[lump_index].name, lump_name, 8);
            lumps[lump_index].lump_pos = current_pos;
            lumps[lump_index].lump_size = patch_size;
            
            // Update tracking
            current_pos += patch_size;
            lump_index++;
            
            // Free patch data
            free(patch_data);
        }
    }
    
    // Write directory entries
    fwrite(lumps, sizeof(LumpEntry), bmp_count, pwad_file);
    
    // Update header with correct directory position
    fseek(pwad_file, 0, SEEK_SET);
    header.directory_pos = current_pos;
    fwrite(&header, sizeof(PWADHeader), 1, pwad_file);
    
    // Cleanup
    fclose(pwad_file);
    free(lumps);
    closedir(dir);
    
    printf("Successfully created %s with %d BMP32 images converted to DOOM patches\n", 
           output_path, bmp_count);
    return 1;
}

void folder_selector_menu() {
    // Semi-transparent background
    glColor4f(0.0, 0.0, 0.5, 0.8);
    glEnable(GL_BLEND);
    glBegin(GL_QUADS);
        glVertex2f(window_width/4, window_height/4);
        glVertex2f(window_width*3/4, window_height/4);
        glVertex2f(window_width*3/4, window_height*3/4);
        glVertex2f(window_width/4, window_height*3/4);
    glEnd();
    glDisable(GL_BLEND);
    
    // Title
    glColor3f(1.0, 1.0, 0.0);
    draw_string(window_width/4 + 20, window_height/4 + 20, "BMP32 to WAD Conversion");
    
    // Input fields
    glColor3f(1.0, 1.0, 1.0);
    draw_string(window_width/4 + 20, window_height/4 + 60, "Input BMP Folder:");
    draw_string(window_width/4 + 20, window_height/4 + 100, input_png_folder);
    
    draw_string(window_width/4 + 20, window_height/4 + 140, "Output WAD Name:");
    draw_string(window_width/4 + 20, window_height/4 + 180, wad_output_name);
    
    draw_string(window_width/4 + 20, window_height/4 + 220, "Output WAD Folder:");
    draw_string(window_width/4 + 20, window_height/4 + 260, output_wad_folder);
    
    // Highlight selected field
    if (selected_input_field == 0) {
        glColor3f(1.0, 1.0, 0.0);
        glBegin(GL_LINE_LOOP);
            glVertex2f(window_width/4 + 20, window_height/4 + 90);
            glVertex2f(window_width*3/4 - 20, window_height/4 + 90);
            glVertex2f(window_width*3/4 - 20, window_height/4 + 110);
            glVertex2f(window_width/4 + 20, window_height/4 + 110);
        glEnd();
    } else if (selected_input_field == 1) {
        glColor3f(1.0, 1.0, 0.0);
        glBegin(GL_LINE_LOOP);
            glVertex2f(window_width/4 + 20, window_height/4 + 130);
            glVertex2f(window_width*3/4 - 20, window_height/4 + 130);
            glVertex2f(window_width*3/4 - 20, window_height/4 + 150);
            glVertex2f(window_width/4 + 20, window_height/4 + 150);
        glEnd();
    } else if (selected_input_field == 2) {
        glColor3f(1.0, 1.0, 0.0);
        glBegin(GL_LINE_LOOP);
            glVertex2f(window_width/4 + 20, window_height/4 + 170);
            glVertex2f(window_width*3/4 - 20, window_height/4 + 170);
            glVertex2f(window_width*3/4 - 20, window_height/4 + 190);
            glVertex2f(window_width/4 + 20, window_height/4 + 190);
        glEnd();
    }
    
    // Convert button
    glColor3f(0.0, 1.0, 0.0);
    draw_string(window_width/4 + 20, window_height/4 + 300, "Press ENTER to Convert");
    
    // Instructions
    glColor3f(0.7, 0.7, 1.0);
    draw_string(window_width/4 + 20, window_height*3/4 - 40, 
        "Use Tab to switch fields, Backspace to edit, Esc to cancel");
}
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "raylib.h"

#define WIDTH  800
#define HEIGHT 600

#define BLINK_DURATION 0.4f

#define FONT_SIZE 25

#define CELL_SIZE 15.0f

#define MIN_POLYGON_DIST 10.0f

#define VEIN_RADIUS 7.0f
#define VEIN_CENTER 2.0f
#define FIRST_VEIN_RADIUS 3*VEIN_RADIUS
#define FIRST_VEIN_OFFSET 2*VEIN_RADIUS

#define AUXIN_RADIUS 10.0f
#define AUXIN_DOT_RADIUS 2.0f
#define AUXIN_SPRAY_THRESHOLD 100

#define LEAF_VEIN_THCK_START 1.0f
#define LEAF_VEIN_THCK_END   5.5f
#define LEAF_COLOR_VEIN1     (Color){175, 189, 34, 170}
#define LEAF_COLOR_VEIN2     (Color){109, 179, 63, 140}
#define LEAF_COLOR_OUTLINE   GREEN
#define LEAF_COLOR_FILL      (Color){0,   97,  14, 180}

#define DA_APPEND(da, item)                                                    \
    do {                                                                       \
        if ((da)->capacity <= (da)->count) {                                   \
            if ((da)->capacity == 0) (da)->capacity = 16;                      \
            else (da)->capacity *= 2;                                          \
            (da)->items = realloc((da)->items, sizeof(*(da)->items) * (da)->capacity); \
            assert((da)->items && "DA_APPEND: realloc failed");                \
        }                                                                      \
        (da)->items[(da)->count++] = (item);                                   \
    } while (0)



const char *hint1 = "Left click to commit leaf's edge";
int hint1_x;
int hint1_y;

const char *hint2 = "Right click to cancel last commited edge";
int hint2_x;
int hint2_y;

const char *hint3 = "Space to attempt to close the loop";
int hint3_x;
int hint3_y;

const char *hint4 = "Cannot finish the loop";
int hint4_x;
int hint4_y;


typedef struct Polygon2 {
    Vector2 *items;
    size_t count;
    size_t capacity;
    bool closed;
} Polygon2;

typedef enum Stage {
    DRAWING,
    GROWING,
    STOPPED,
} Stage;

typedef struct Indeces {
    size_t *items;
    size_t count;
    size_t capacity;
} Indeces;

typedef struct Node {
    Vector2 center;
    float radius;
    Indeces closest_source_indeces;
    Vector2 parent;
} Node;

typedef struct Veins {
    Node *items;
    size_t count;
    size_t capacity;
} Veins;

typedef struct Branch {
    Vector2 *items;
    size_t count;
    size_t capacity;
} Branch;

typedef struct Tree {
    Branch *items;
    size_t count;
    size_t capacity;
} Tree;

typedef struct Source {
    Vector2 center;
    float radius;
    size_t closest_node_index;
} Source;

typedef struct Auxins {
    Source *items;
    size_t count;
    size_t capacity;
} Auxins;

typedef struct Edge2 {
    Vector2 a, b;
} Edge2;

typedef struct Triangle2 {
    Vector2 a, b, c;
} Triangle2;

typedef struct Vertices {
    Triangle2 *items;
    size_t count;
    size_t capacity;
} Vertices;


static bool write_polygon_file(const Polygon2 *polygon, const char *file_name) {
    FILE *file_desc = fopen(file_name, "wb");
    if (!file_desc) {
        printf("Can't open file: %s\n", file_name);
        return false;
    }
    if (fwrite(&polygon->count, sizeof(polygon->count), 1, file_desc) != 1) {
        printf("Can't write amount of polygon points to file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    if (fwrite(polygon->items, sizeof(Vector2), polygon->count, file_desc) != polygon->count) {
        printf("Can't write a polygon points to file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    if (fclose(file_desc) != 0) {
        printf("Can't close file: %s\n", file_name);
        return false;
    }
    printf("Polygon saved to file: %s\n", file_name);
    return true;
}

static bool read_polygon_file(Polygon2 *polygon, const char *file_name) {
    FILE *file_desc = fopen(file_name, "rb");
    if (!file_desc) {
        printf("Can't open file: %s\n", file_name);
        return false;
    }
    if (fread(&polygon->count, sizeof(polygon->count), 1, file_desc) != 1) {
        printf("Can't read number of a polygon points from file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    polygon->capacity = polygon->count;
    polygon->items = realloc(polygon->items, sizeof(Vector2) * polygon->capacity);
    if (fread(polygon->items, sizeof(Vector2), polygon->count, file_desc) != polygon->count) {
        printf("Can't read polygon points from file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    if (fclose(file_desc) != 0) {
        printf("Can't close file: %s\n", file_name);
        return false;
    }
    printf("Polygon read from file: %s\n", file_name);
    return true;
}

static bool read_veins_file(Veins *veins, const char *file_name) {
    FILE *file_desc = fopen(file_name, "rb");
    if (!file_desc) {
        printf("Can't open file: %s\n", file_name);
        return false;
    }
    if (fread(&veins->count, sizeof(veins->count), 1, file_desc) != 1) {
        printf("Can't read number of a veins nodes from file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    veins->capacity = veins->count;
    veins->items = realloc(veins->items, sizeof(Node) * veins->capacity);
    if (fread(veins->items, sizeof(Node), veins->count, file_desc) != veins->count) {
        printf("Can't read veins nodes from file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    if (fclose(file_desc) != 0) {
        printf("Can't close file: %s\n", file_name);
        return false;
    }
    printf("Veins read from file: %s\n", file_name);
    return true;
}

static bool write_veins_file(const Veins *veins, const char *file_name) {
    FILE *file_desc = fopen(file_name, "wb");
    if (!file_desc) {
        printf("Can't open file: %s\n", file_name);
        return false;
    }
    if (fwrite(&veins->count, sizeof(veins->count), 1, file_desc) != 1) {
        printf("Can't write amount of veins nodes to file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    if (fwrite(veins->items, sizeof(Node), veins->count, file_desc) != veins->count) {
        printf("Can't write a veins nodes to file: %s\n", file_name);
        fclose(file_desc);
        return false;
    }
    if (fclose(file_desc) != 0) {
        printf("Can't close file: %s\n", file_name);
        return false;
    }
    printf("Veins saved to file: %s\n", file_name);
    return true;
}


static inline bool same_v2(Vector2 a, Vector2 b) { return a.x == b.x && a.y == b.y; }

static inline Vector2 sub_v2(Vector2 a, Vector2 b) {
    return (Vector2){a.x - b.x, a.y - b.y};
}

static inline Vector2 add_v2(Vector2 a, Vector2 b) {
    return (Vector2){a.x + b.x, a.y + b.y};
}

static inline Vector2 scale_v2(Vector2 a, float s) {
    return (Vector2){a.x * s, a.y * s};
}

static inline float len_v2_sq(Vector2 ab) {
    return ab.x * ab.x + ab.y * ab.y;
}

static inline float len_v2(Vector2 ab) {
    return sqrt(len_v2_sq(ab));
}

static inline float cross_v2(Vector2 ab, Vector2 ap) {
    return ab.x * ap.y - ab.y * ap.x;
}

static inline Color lerp_color(Color a, Color b, float p) {
    return (Color) {
        (unsigned char)(a.r + (b.r - a.r)*p),
        (unsigned char)(a.g + (b.g - a.g)*p),
        (unsigned char)(a.b + (b.b - a.b)*p),
        (unsigned char)(a.a + (b.a - a.a)*p),
    };
}

static Vector2 norm_v2(Vector2 v) {
    // Calculating on double precision to check for overflow
    double len = sqrt(v.x * v.x + v.y * v.y);
    if (fabs(len) > (double)__FLT_MAX__) return (Vector2){0, 0};
    float inv = 1.0f / len;
    return (Vector2){v.x * inv, v.y * inv};
}

// Closest point on a line defined by segment AB to point C, if clampled - strictly on segment
static Vector2 closest_on_segment(Vector2 a, Vector2 b, Vector2 c, bool clamped) {
    Vector2 ab = sub_v2(b, a);
    Vector2 ac = sub_v2(c, a);
    float t = (ac.x * ab.x + ac.y * ab.y) / len_v2_sq(ab);
    // clamp
    if (clamped) t = t < 0 ? 0.0f : (t > 1 ? 1 : t);
    return (Vector2){a.x + t * ab.x, a.y + t * ab.y};
}

// Distance from C to a line defined by segment AB, if clampled - strictly to segment
static float distance_v2(Vector2 a, Vector2 b, Vector2 c, bool clamped) {
    if (same_v2(a, b)) return len_v2(sub_v2(a, c));
    return len_v2(sub_v2(c, closest_on_segment(a, b, c, clamped)));
}

static inline int sign(float x) { return (x > 0) - (x < 0); }

static int orient(Vector2 a, Vector2 b, Vector2 c) {
    // orientation of triplet (a,b,c): >0 left, <0 right, 0 collinear
    return sign(cross_v2(sub_v2(b, a), sub_v2(c, a)));
}

static int on_segment(Vector2 a, Vector2 b, Vector2 p) {
    // assumes p is collinear with a-b; check bounding box
    return (p.x >= fminf(a.x, b.x) && p.x <= fmaxf(a.x, b.x)
         && p.y >= fminf(a.y, b.y) && p.y <= fmaxf(a.y, b.y));
}

bool segments_intersect(Vector2 a1, Vector2 a2, Vector2 b1, Vector2 b2) {
    int o1 = orient(a1, a2, b1); // where b1 relative to A
    int o2 = orient(a1, a2, b2); // where b2 relative to A
    int o3 = orient(b1, b2, a1); // where a1 relative to B
    int o4 = orient(b1, b2, a2); // where a2 relative to B

    if (o1 != o2 && o3 != o4)
        return true; // proper intersection

    // special collinear cases
    if (o1 == 0 && on_segment(a1, a2, b1)) return true;
    if (o2 == 0 && on_segment(a1, a2, b2)) return true;
    if (o3 == 0 && on_segment(b1, b2, a1)) return true;
    if (o4 == 0 && on_segment(b1, b2, a2)) return true;

    return false;
}

static bool segment_intersect_polygon(Vector2 a, Vector2 b, const Polygon2 *polygon) {
    if (same_v2(a, b)) return false;      // zero lenght segment in
    if (polygon->count < 2) return false; // no actual segments to test against in the list

    // Skip last edge if polygon not yet closed
    size_t max_index = polygon->closed ? polygon->count : polygon->count - 1;

    for (size_t i = 0; i < max_index; i++) {
        Vector2 c = polygon->items[i];
        Vector2 d = polygon->items[(i + 1) % polygon->count];
        assert(!same_v2(c, d) && "Leaf contain zero lenght segment");
        bool a_is_c = same_v2(a, c);
        bool a_is_d = same_v2(a, d);
        bool b_is_c = same_v2(b, c);
        bool b_is_d = same_v2(b, d);

        // Segments fully match
        if ((a_is_c && b_is_d) || (a_is_d && b_is_c)) continue;

        // Segments are sequential
        if (a_is_c || a_is_d) {
            // Test B distance to CD
            if (distance_v2(c, d, b, true) < MIN_POLYGON_DIST) return true;
            continue;
        }
        if (b_is_c || b_is_d) {
            // Test A distance to CD
            if (distance_v2(c, d, a, true) < MIN_POLYGON_DIST) return true;
            continue;
        }

        // General case
        if (segments_intersect(a, b, c, d)) return true;
    }
    return false;
}

// true if finished
static bool drawing(Polygon2 *polygon, Vector2 seed_coord, int width, int height) {
    static double dt;
    static bool blink_happening = false;

    // draw cells for easier drawing
    for (int x = CELL_SIZE; x < width; x += CELL_SIZE)
        DrawLine(x, 0, x, height, (Color){ 130, 130, 130, 155 });
    for (int y = CELL_SIZE; y < height; y += CELL_SIZE)
        DrawLine(0, y, width, y, (Color){ 130, 130, 130, 155 }); // 200, 200, 200, 255

    dt += GetFrameTime();
    bool polygon_valid;
    Vector2 mouse = GetMousePosition();

    // Last commited point
    Vector2 p = polygon->items[polygon->count - 1];

    if (IsKeyPressed(KEY_SPACE)) {
        polygon_valid = polygon->count >= 3;
        // Check if the line from the last commited point to the first intersect the vein
        if (polygon_valid) polygon_valid = distance_v2(p, polygon->items[0], seed_coord, true) > FIRST_VEIN_RADIUS;
        // Or any previous edge
        if (polygon_valid) polygon_valid = !segment_intersect_polygon(p, polygon->items[0], polygon);
        if (polygon_valid) return true; // drawing finished
        // else
        dt = 0;
        blink_happening = true;
    }

    Color bg_color = LIGHTGRAY;
    if (dt >= BLINK_DURATION) blink_happening = false;
    if (blink_happening) {
        bg_color = (Color){ 230, 41, 55, 100 };
        DrawText(hint4, hint4_x, hint4_y, 2 * FONT_SIZE, (Color){ 120, 120, 120, 255 });
    }
    ClearBackground(bg_color); BLACK;

    DrawText(hint1, hint1_x, hint1_y, FONT_SIZE, (Color){ 10, 10, 10, 180 });
    DrawText(hint2, hint2_x, hint2_y, FONT_SIZE, (Color){ 10, 10, 10, 180 });
    DrawText(hint3, hint3_x, hint3_y, FONT_SIZE, (Color){ 10, 10, 10, 180 });

    DrawCircle(seed_coord.x, seed_coord.y, FIRST_VEIN_RADIUS, WHITE);
    DrawCircle(seed_coord.x, seed_coord.y, VEIN_CENTER, BLACK);

    polygon_valid = len_v2(sub_v2(mouse, p)) > MIN_POLYGON_DIST;
    if (polygon_valid) polygon_valid = distance_v2(p, mouse, seed_coord, true) > FIRST_VEIN_RADIUS;
    if (polygon_valid) polygon_valid = !segment_intersect_polygon(p, mouse, polygon);

    for (size_t i = 0; i < polygon->count - 1; i++) {
        DrawLineEx(polygon->items[i], polygon->items[i + 1], 2.0f, GREEN);
    }

    Color color = polygon_valid ? GREEN : RED;
    DrawLineEx(p, mouse, 2.0f, color);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Commit new point if new edge does not intersect anything
        if (polygon_valid) DA_APPEND(polygon, mouse);
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && polygon->count > 1) {
        polygon->count--;
    }
    return false;
}

static bool point_inside(Vector2 point, const Polygon2 *polygon, float *min_dist) {
    int winding_number = 0;
    *min_dist = __FLT_MAX__;

    for (size_t i = 0; i < polygon->count; i++) {
        Vector2 start = polygon->items[i];
        Vector2 end = polygon->items[(i + 1) % polygon->count];

        float dist = distance_v2(start, end, point, true);
        if (*min_dist > dist) *min_dist = dist;

        Vector2 ab = sub_v2(end, start);
        Vector2 ap = sub_v2(point, start);

        if (start.y <= point.y && end.y > point.y) {
            if (cross_v2(ab, ap) > 0) winding_number += 1;
        } else if (start.y > point.y && end.y <= point.y) {
            if (cross_v2(ab, ap) < 0) winding_number -= 1;
        }
    }

    return winding_number != 0;  // Non-zero means inside
}

static void spray_auxins(Auxins *auxins, const Polygon2 *polygon, int width, int height) {
    while (auxins->count < AUXIN_SPRAY_THRESHOLD) {
        Vector2 p = {GetRandomValue(0, width), GetRandomValue(0, height)};
        float min_dist;
        if (point_inside(p, polygon, &min_dist) && min_dist > MIN_POLYGON_DIST) {
            assert(min_dist < __FLT_MAX__ && "A point inside the shape should have finite minimal distance");
            DA_APPEND(auxins, ((Source){.center=p, .radius=AUXIN_RADIUS, .closest_node_index=0}));
        }
    }
}

static void eliminate_auxins(Auxins *auxins, const Veins *veins, const Polygon2 *polygon) {
    static Indeces to_remove = (Indeces){0};
    to_remove.count = 0; // DON'T FORGET TO ZERO COUNT

    // Collect indeces to remove
    for (size_t i = 0; i < auxins->count; i++) {
        float min_dist;
        if (!point_inside(auxins->items[i].center, polygon, &min_dist) || min_dist < AUXIN_RADIUS / 2) {
            DA_APPEND((&to_remove), i);
            continue;
        }
        for (size_t j = 0; j < veins->count; j++) {
            // Take into account that thier might not be a direct path, yet it probably does not matter much
            if (len_v2(sub_v2(auxins->items[i].center, veins->items[j].center)) <= auxins->items[i].radius &&
                !segment_intersect_polygon(auxins->items[i].center, veins->items[j].center, polygon)) {
                DA_APPEND((&to_remove), i);
                break;
            }
        }
    }

    // Remove in reverse order using swap with last item and count decrement
    assert(to_remove.count <= auxins->count && "Attempting to remove more auxins then exist");
    for (size_t i = to_remove.count; i-- > 0;) {
        size_t auxin_idx = to_remove.items[i];
        auxins->items[auxin_idx] = auxins->items[auxins->count - 1];
        auxins->count--;
    }
}

static void associate_auxins(Auxins *auxins, Veins *veins, const Polygon2 *polygon) {
    static Indeces to_remove = (Indeces){0};
    to_remove.count = 0; // DON'T FORGET TO ZERO COUNT

    // Find closest node for each auxin
    for (size_t i = 0; i < auxins->count; i++) {
        float min_dist = __FLT_MAX__;
        auxins->items[i].closest_node_index = veins->count;
        for (size_t j = 0; j < veins->count; j++) {
            float dist = len_v2(sub_v2(auxins->items[i].center, veins->items[j].center));
            if (min_dist > dist &&
                // Thier might not be direct path
                !segment_intersect_polygon(auxins->items[i].center, veins->items[j].center, polygon)) {
                min_dist = dist;
                auxins->items[i].closest_node_index = j;
            }
        }

        // Remove auxins that have no direct path to any vein node
        if (auxins->items[i].closest_node_index >= veins->count)
            DA_APPEND((&to_remove), i);
    }

    // Remove in reverse order using swap with last item and count decrement
    assert(to_remove.count <= auxins->count && "Attempting to remove more auxins then exist");
    for (size_t i = to_remove.count; i-- > 0;) {
        size_t auxin_idx = to_remove.items[i];
        auxins->items[auxin_idx] = auxins->items[auxins->count - 1];
        auxins->count--;
    }

    // Clear previous closest sources
    for (size_t i = 0; i < veins->count; i++) {
        veins->items[i].closest_source_indeces.count = 0;
    }

    // Associate auxins with correcponding nodes
    for (size_t i = 0; i < auxins->count; i++) {
        size_t vein_idx = auxins->items[i].closest_node_index;
        assert(vein_idx < veins->count && "Invalid auxin leaked into association");
        DA_APPEND((&veins->items[vein_idx].closest_source_indeces), i);
    }

}

static Edge2 closest_edge(Vector2 point, const Polygon2 *polygon, float *min_dist) {
    Edge2 edge = {0};
    *min_dist = __FLT_MAX__;

    for (size_t i = 0; i < polygon->count; i++) {
        Vector2 start = polygon->items[i];
        Vector2 end = polygon->items[(i + 1) % polygon->count];

        float dist = distance_v2(start, end, point, true);
        if (*min_dist > dist) {
            *min_dist = dist;
            edge.a = start;
            edge.b = end;
        }
    }

    return edge;
}

static void produce_new_nodes(const Auxins *auxins, const Polygon2 *polygon, Veins *veins) {
    size_t veins_cnt = veins->count;
    for (size_t i = 0; i < veins_cnt; i++) {
        const Node *node = &veins->items[i];
        // Skip nodes with no connections
        if (node->closest_source_indeces.count == 0) continue;

        // Calculate normalized sum
        Vector2 normalized_sum = (Vector2){0};
        for (size_t j = 0; j < node->closest_source_indeces.count; j++) {
            size_t auxin_idx = node->closest_source_indeces.items[j];
            normalized_sum = add_v2(normalized_sum, norm_v2(sub_v2(auxins->items[auxin_idx].center, node->center)));
        }
        normalized_sum = norm_v2(normalized_sum); // Final normalization

        // Add new vein node
        Vector2 new_center = add_v2(node->center, scale_v2(normalized_sum, 2 * node->radius));

        // Hackity hack to keep nodes further from edges
        // Makes whole process very unstable, sadge
        // float min_dist;
        // Edge2 closest = closest_edge(new_center, polygon, &min_dist);
        // if (min_dist < MIN_POLYGON_DIST) {
        //     // Point on an infinite line defined by the edge closest to new center
        //     Vector2 p = closest_on_segment(closest.a, closest.b, new_center, false);
        //     Vector2 repulsion = scale_v2(norm_v2(sub_v2(new_center, p)), min_dist / MIN_POLYGON_DIST);
        //     normalized_sum = norm_v2(add_v2(normalized_sum, repulsion));
        //     new_center = add_v2(node->center, scale_v2(normalized_sum, 2 * node->radius));
        // }

        DA_APPEND(veins, ((Node){.center=new_center, .radius=VEIN_RADIUS, .closest_source_indeces=(Indeces){0}, .parent=node->center}));
    }
}

static void construct_vertices(Vertices *vertices, const Polygon2 *polygon) {
    size_t points_count = polygon->count;
    Vector2 *points = malloc(points_count * sizeof(Vector2));
    memcpy(points, polygon->items, points_count * sizeof(Vector2));

    size_t i = 0;
    while (points_count > 3) {
        Vector2 curr = points[i];
        Vector2 next = points[(i + 1) % points_count];
        Vector2 prev = points[(i == 0 ? points_count - 1 : i - 1)];
        // Ear Clipping: test if the edge from neighboring points inside the polygon
        Vector2 test_edge = sub_v2(prev, next);
        Vector2 test_point = {next.x + test_edge.x / 2, next.y + test_edge.y / 2};
        float min_dist;
        if (point_inside(test_point, polygon, &min_dist) && !segment_intersect_polygon(next, prev, polygon)) {
            assert(min_dist < __FLT_MAX__ && "A point inside the shape should have finite minimal distance");
            float cross = cross_v2(sub_v2(next, curr), sub_v2(prev, curr));
            Triangle2 t = (cross < 0 ? (Triangle2){curr, next, prev} : (Triangle2){curr, prev, next});
            DA_APPEND(vertices, t);

            // Remove curr point without re-shuffle, sadge
            for (size_t j = i; j < points_count - 1; j++) {
                points[j] = points[j + 1];
            }
            points_count--;
        } else {
            i++;
        }
        // Rotate index around
        i = i % points_count;
    }
    // And last triangle
    Vector2 curr = points[0];
    Vector2 next = points[1];
    Vector2 prev = points[2];
    float cross = cross_v2(sub_v2(next, curr), sub_v2(prev, curr));
    Triangle2 t = (cross < 0 ? (Triangle2){curr, next, prev} : (Triangle2){curr, prev, next});
    DA_APPEND(vertices, t);
    free(points);
}

static Node* node_by_parent(const Veins *veins, Vector2 parent) {
    for (size_t i = 0; i < veins->count; i++) {
        if (same_v2(parent, veins->items[i].center)) return &veins->items[i]; 
    }
    printf("parent: (%.3f, %.3f)\n", parent.x, parent.y);
    assert(!"Unreacheble");
}

// Traverse vein nodes and create tree structure
static void traverse_parents(const Veins *veins, Vector2 root, Tree *tree) {
    Indeces leaves = (Indeces){0};
    for (size_t i = 0; i < veins->count; i++) {
        bool used_as_parent = false;
        const Node *node = &veins->items[i];
        for (size_t j = 0; j < veins->count; j++) {
            if (same_v2(veins->items[j].parent, node->center)) {
                used_as_parent = true;
                break;
            }
        }
        if (!used_as_parent)
            DA_APPEND(&leaves, i);
    }

    for (size_t i = 0; i < leaves.count; i++) {
        Branch v = (Branch){0};
        Node *node = &veins->items[leaves.items[i]];
        DA_APPEND(&v, node->center);
        while (!same_v2(node->parent, (Vector2){-1, -1})) {
            node = node_by_parent(veins, node->parent);
            DA_APPEND((&v), node->center);
        }
        // Finish branch with the root point
        DA_APPEND((&v), root);
        DA_APPEND(tree, v);
    }

    free(leaves.items);
}

int main(void) {
    InitWindow(WIDTH, HEIGHT, "Leafer");
    SetTargetFPS(60);
    unsigned int seed_val = GetRandomValue(0, 0x7FFFFFFF);
    printf("Seed: %d\n", seed_val);
    SetRandomSeed(seed_val);

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    Stage current_stage = DRAWING;

    Polygon2 polygon = (Polygon2){0};
    polygon.closed = false;
    DA_APPEND(&polygon, ((Vector2){width / 2, height - FIRST_VEIN_OFFSET / 2}));

    Vector2 seed_coord = {width / 2, height - FIRST_VEIN_RADIUS - FIRST_VEIN_OFFSET};

    Veins veins = (Veins){0};
    DA_APPEND(&veins, ((Node){(Vector2){seed_coord.x - VEIN_RADIUS, seed_coord.y - VEIN_RADIUS}, VEIN_RADIUS, .closest_source_indeces = (Indeces){0}, .parent=(Vector2){-1, -1}}));
    DA_APPEND(&veins, ((Node){seed_coord,                                                        VEIN_RADIUS, .closest_source_indeces = (Indeces){0}, .parent=veins.items[0].center}));
    DA_APPEND(&veins, ((Node){(Vector2){seed_coord.x + VEIN_RADIUS, seed_coord.y + VEIN_RADIUS}, VEIN_RADIUS, .closest_source_indeces = (Indeces){0}, .parent=veins.items[1].center}));

    Auxins auxins = (Auxins){0};
    Vertices vertices = (Vertices){0};
    Tree tree = (Tree){0};

    // define set of global vars
    hint1_x = (width - strlen(hint1) * FONT_SIZE / 2) / 2;
    hint1_y = height / 3 - FONT_SIZE;
    hint2_x = (width - strlen(hint2) * FONT_SIZE / 2) / 2;
    hint2_y = height / 3;
    hint3_x = (width - strlen(hint3) * FONT_SIZE / 2) / 2;
    hint3_y = height / 3 + FONT_SIZE;
    hint4_x = (width - strlen(hint4) * FONT_SIZE) / 2;
    hint4_y = height / 3 + 4 * FONT_SIZE;

    int final_attemps = 100;
    while (!WindowShouldClose()) {
        BeginDrawing();

        switch (current_stage) {
            case DRAWING: {
                if (drawing(&polygon, seed_coord, width, height)) {
                    if (!write_polygon_file(&polygon, "polygon_out.bin")) return 1;
                    polygon.closed = true;
                    current_stage = GROWING;
                }
            } break;
            case GROWING: {
                ClearBackground(LIGHTGRAY);

                for (size_t i = 0; i < polygon.count; i++) {
                    DrawLineEx(polygon.items[i], polygon.items[(i + 1) % polygon.count], 3.0f, GREEN);
                }

                for (size_t i = 0; i < veins.count; i++) {
                    DrawCircle(veins.items[i].center.x, veins.items[i].center.y, veins.items[i].radius, WHITE);
                    DrawCircle(veins.items[i].center.x, veins.items[i].center.y, VEIN_CENTER, BLACK);
                }

                // 1. If number of auxin is lower then spray threshold -> spray more
                spray_auxins(&auxins, &polygon, width, height);

                // 2. Remove auxins which radius fits in any vein's center, draw them after
                eliminate_auxins(&auxins, &veins, &polygon);

                // At some point the growth stops and no new auxin will survive elimination
                // Move to next stage after certain amount of last attempts
                if (auxins.count == 0 && --final_attemps) {
                    write_veins_file(&veins, "veins_out.bin");
                    construct_vertices(&vertices, &polygon);
                    traverse_parents(&veins, polygon.items[0], &tree);
                    current_stage = STOPPED;
                    break; // break of the switch
                }

                for (size_t i = 0; i < auxins.count; i++) {
                    DrawCircle(auxins.items[i].center.x, auxins.items[i].center.y, AUXIN_DOT_RADIUS, PINK);
                    DrawRing(auxins.items[i].center, auxins.items[i].radius, auxins.items[i].radius + 1, 0, 360, 200, PINK);
                }

                // 4. Associate each auxin with vein node that is closes to it
                associate_auxins(&auxins, &veins, &polygon);

                // 5. Construct normalized vectors from the vein node to each associated auzin source
                // 6. Sum constructed vectors and normalize it again -> calculate location of new node and add them
                produce_new_nodes(&auxins, &polygon, &veins);
            } break;
            case STOPPED: {
                ClearBackground(LIGHTGRAY);

                for (size_t i = 0; i < vertices.count; i++) {
                    const Triangle2 *t = &vertices.items[i];
                    DrawTriangle(t->a, t->b, t->c, LEAF_COLOR_FILL);
                }

                for (size_t i = 0; i < polygon.count; i++) {
                    DrawLineEx(polygon.items[i], polygon.items[(i + 1) % polygon.count], 3.0f, LEAF_COLOR_OUTLINE);
                }

                for (size_t i = 0; i < tree.count; i++) {
                    const Branch *v = &tree.items[i];
                    for (size_t j = 0; j < v->count - 1; j++) {
                        float t = (float)j / (float)v->count;
                        float thickness = LEAF_VEIN_THCK_START + (LEAF_VEIN_THCK_END - LEAF_VEIN_THCK_START) * t;
                        Color color = lerp_color(LEAF_COLOR_VEIN1, LEAF_COLOR_VEIN2, t);
                        DrawLineEx(v->items[j], v->items[j + 1], thickness, color);
                    }
                }
            } break;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
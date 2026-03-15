import taichi as ti

ti.init(arch=ti.gpu)

# --- Configuration ---
res = 800
n_particles = 15000
h = 0.05                # Interaction radius
grid_size = int(1.0 / h) # Number of grid cells per axis

# Physics constants
dt = 1e-3
padding = 0.1

# --- Fields ---
x = ti.Vector.field(3, dtype=float, shape=n_particles)
v = ti.Vector.field(3, dtype=float, shape=n_particles)

# Grid for Spatial Partitioning
# Stores which particle is in which cell
grid_num_particles = ti.field(int, shape=(grid_size, grid_size, grid_size))
grid2particles = ti.field(int, shape=(grid_size, grid_size, grid_size, 100)) # Max 100 particles per cell

@ti.kernel
def init_particles():
    for i in range(n_particles):
        x[i] = ti.Vector([ti.random() * 0.3 + 0.3, 
                          ti.random() * 0.5 + 0.2, 
                          ti.random() * 0.3 + 0.3])
        v[i] = ti.Vector([0.0, 0.0, 0.0])

@ti.kernel
def update_grid():
    # Clear grid
    grid_num_particles.fill(0)
    for i in range(n_particles):
        # Calculate cell index
        cell = (x[i] * grid_size).cast(int)
        # Clamp to grid bounds
        cell = ti.max(0, ti.min(cell, grid_size - 1))
        
        idx = ti.atomic_add(grid_num_particles[cell], 1)
        if idx < 100:
            grid2particles[cell, idx] = i

@ti.kernel
def move():
    for i in x:
        # Gravity
        v[i].y -= 9.8 * dt
        
        # Grid-based neighbor check (Placeholder for SPH forces)
        # Instead of checking 15,000 particles, we only check neighbors
        cell = (x[i] * grid_size).cast(int)
        
        # Boundary Collisions (The Box)
        for j in ti.static(range(3)):
            if x[i][j] < padding:
                x[i][j] = padding
                v[i][j] *= -0.5
            if x[i][j] > 1.0 - padding:
                x[i][j] = 1.0 - padding
                v[i][j] *= -0.5

        x[i] += v[i] * dt

def main():
    init_particles()
    window = ti.ui.Window("Optimized Water", (res, res))
    canvas = window.get_canvas()
    scene = window.get_scene()
    camera = ti.ui.Camera()
    camera.position(2, 2, 2)
    camera.lookat(0.5, 0.5, 0.5)

    while window.running:
        # Run physics multiple times per frame for stability
        for _ in range(5):
            update_grid()
            move()
        
        camera.track_user_inputs(window, movement_speed=0.03, hold_key=ti.ui.RMB)
        scene.set_camera(camera)
        scene.point_light(pos=(2, 2, 2), color=(1, 1, 1))
        
        # Render as particles
        scene.particles(x, radius=0.008, color=(0.2, 0.6, 1.0))
        canvas.scene(scene)
        window.show()

if __name__ == "__main__":
    main()

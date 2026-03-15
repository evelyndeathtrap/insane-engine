var THREE = require("three");

let player;
let camera;
let isPointerLocked = false;
let cameraPitch = 0;
let visualModel;
const keys = {};

function init() {
    player = this;
    
    findVisualModel();
    findCamera();
    
    // Set rotation order to prevent Gimbal Lock
    player.rotation.reorder('YXZ');

    // Start animations
    if (this.animations && this.animations.idle) {
        this.animations.idle.play();
    }
    
    // Pointer Lock
    player.renderer.domElement.addEventListener("click", async () => {
        try {
            await player.renderer.domElement.requestPointerLock();
        } catch (err) {
            console.log("Pointer lock failed", err);
        }
    });
    
    document.addEventListener("pointerlockchange", () => {
        isPointerLocked = document.pointerLockElement === player.renderer.domElement;
    });

    // Inputs
    window.addEventListener("keydown", e => keys[e.keyCode] = true);
    window.addEventListener("keyup", e => keys[e.keyCode] = false);

    // Mouse Look
    document.addEventListener("mousemove", e => {
        if (!isPointerLocked) return;
        
        // 1. Horizontal: Rotate the player object
        player.rotation.y -= e.movementX * 0.002;
        
        // 2. Vertical: Pitch the camera
        cameraPitch -= e.movementY * 0.002;
        cameraPitch = Math.max(-Math.PI/2, Math.min(Math.PI/2, cameraPitch));
        
        if (camera) {
            // If camera is inside an "Axis" object, rotate that, else rotate camera
            const pitchTarget = (camera.parent && camera.parent.name === "Axis") ? camera.parent : camera;
            pitchTarget.rotation.x = cameraPitch;
        }

        // 3. Update Physics Body Rotation immediately
        forcePhysicsRotationUpdate();
    });
}

function handleMovement(delta) {
    if (!player || !player.physicsBody) return;
    
    const body = player.physicsBody;
    const speed = 8;
    const moveDir = new THREE.Vector3(0, 0, 0);

    // Calculate directions relative to current player rotation
    const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(player.quaternion);
    const right = new THREE.Vector3(1, 0, 0).applyQuaternion(player.quaternion);
    
    // Flatten vectors to prevent flying/sinking
    forward.y = 0; forward.normalize();
    right.y = 0; right.normalize();

    if (keys[87]) moveDir.add(forward); // W
    if (keys[83]) moveDir.sub(forward); // S
    if (keys[65]) moveDir.sub(right);   // A
    if (keys[68]) moveDir.add(right);   // D

    const currentVel = body.getLinearVelocity();
    let targetVelX = 0;
    let targetVelZ = 0;

    if (moveDir.length() > 0) {
        moveDir.normalize().multiplyScalar(speed);
        targetVelX = moveDir.x;
        targetVelZ = moveDir.z;

        if (player.animations.running && !player.animations.running.isRunning()) {
            player.animations.idle?.stop();
            player.animations.running.play();
        }
    } else {
        if (player.animations.idle && !player.animations.idle.isRunning()) {
            player.animations.running?.stop();
            player.animations.idle.play();
        }
    }

    // Apply velocity while preserving Gravity (Y)
    const newVel = new player.ammo.btVector3(targetVelX, currentVel.y(), targetVelZ);
    body.setLinearVelocity(newVel);
    player.ammo.destroy(newVel);
}

function forcePhysicsRotationUpdate() {
    const body = player.physicsBody;
    const transform = new player.ammo.btTransform();
    body.getMotionState().getWorldTransform(transform);

    const quat = new THREE.Quaternion().setFromEuler(player.rotation);
    const ammoQuat = new player.ammo.btQuaternion(quat.x, quat.y, quat.z, quat.w);

    transform.setRotation(ammoQuat);
    body.setCenterOfMassTransform(transform);
    body.getMotionState().setWorldTransform(transform);
    body.activate(); // Prevent body from "sleeping"

    player.ammo.destroy(ammoQuat);
    player.ammo.destroy(transform);
}

function update(delta) {
    if (player && player.physicsBody) {
        const body = player.physicsBody;
        const ms = body.getMotionState();
        if (ms) {
            const trans = new player.ammo.btTransform();
            ms.getWorldTransform(trans);
            const p = trans.getOrigin();
            const q = trans.getRotation();
            
            // Sync Visuals to Physics
            player.position.set(p.x(), p.y(), p.z());
            // We usually only sync position; we set rotation manually via Mouse
            
            player.ammo.destroy(trans);
        }
        handleMovement(delta);
    }

    if (player.mixer) player.mixer.update(delta);
}

// Helper Finders
function findVisualModel() {
    player.traverse(obj => {
        if (obj.isMesh && obj !== player && !obj.isCamera) visualModel = obj;
    });
    if (!visualModel) visualModel = player;
}

function findCamera() {
    player.traverse(obj => { if (obj.isCamera) camera = obj; });
}

module.exports = { init, update };

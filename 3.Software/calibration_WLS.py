import numpy as np
from scipy.optimize import least_squares
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import pandas as pd
import warnings

# Configure matplotlib to avoid font warnings
try:
    # Set default font to avoid Chinese character issues
    plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial']
    plt.rcParams['axes.unicode_minus'] = False
    
    # Suppress font-related warnings
    warnings.filterwarnings('ignore', category=UserWarning, module='matplotlib')
    warnings.filterwarnings('ignore', category=UserWarning, module='tkinter')
    warnings.filterwarnings('ignore', message='.*Glyph.*missing from current font.*')
    
except Exception as e:
    print(f"Font configuration failed: {e}")

keyboard_coordinates = {
    "Esc": (0, 0),
    "1": (19.125, 0),
    "2": (38.175, 0),
    "3": (57.225, 0),
    "4": (76.275, 0),
    "5": (95.325, 0),
    "6": (114.375, 0),
    "7": (133.425, 0),
    "8": (152.475, 0),
    "9": (171.525, 0),
    "0": (190.575, 0),
    "-": (209.625, 0),
    "Backspace": (257.305, 0),
    "Q": (28.645, -19.55),
    "W": (47.695, -19.55),
    "E": (66.745, -19.55),
    "R": (85.795, -19.55),
    "T": (104.845, -19.55),
    "Y": (123.895, -19.55),
    "U": (142.945, -19.55),
    "I": (161.995, -19.55),
    "O": (181.045, -19.55),
    "P": (200.095, -19.55),
    "CapsLock": (7.15, -38.6),
    "A": (33.405, -38.6),
    "S": (52.455, -38.6),
    "D": (71.505, -38.6),
    "F": (90.555, -38.6),
    "G": (109.605, -38.6),
    "H": (128.655, -38.6),
    "J": (147.705, -38.6),
    "K": (166.755, -38.6),
    "L": (185.805, -38.6),
    ";": (204.855, -38.6),
    "'": (223.905, -38.6),
    "Enter": (254.925, -38.6),
    "Shift": (11.915, -57.65),
    "Z": (42.935, -57.65),
    "X": (61.985, -57.65),
    "C": (81.035, -57.65),
    "V": (100.085, -57.65),
    "B": (119.135, -57.65),
    "N": (138.185, -57.65),
    "M": (157.235, -57.65),
    ",": (176.285, -57.65),
    ".": (195.335, -57.65),
    "/": (214.385, -57.65),
    "Ctrl": (1.19, -77.08),
    "OS": (26.265, -77.08),
    "Alt": (50.075, -77.08),
    "Space": (121.515, -77.08)
}

def get_key_coordinates(key):
    return np.array(keyboard_coordinates[key])

def residuals(params, key_pairs):
    dx, dy, theta = params
    residual = []
    
    for original_key, observed_key, confidence in key_pairs:
        p_original = get_key_coordinates(original_key)
        p_observed = get_key_coordinates(observed_key)
        
        # Rotation matrix
        rotation_matrix = np.array([[np.cos(theta), -np.sin(theta)],
                                    [np.sin(theta), np.cos(theta)]])
        
        # Calculate expected position after transformation
        p_transformed = rotation_matrix @ p_original + np.array([dx, dy])
        
        # Calculate residual with weighting
        res = (p_observed - p_transformed) * confidence
        residual.append(res)
    
    return np.concatenate(residual)

def estimate_transform(key_pairs, allow_rotation=True):
    """Estimate transformation parameters
    
    Args:
        key_pairs: List of key pairs, each containing (original_key, observed_key, confidence)
        allow_rotation: Whether to allow rotation transformation, if False theta is fixed to 0
    
    Returns:
        dx, dy, theta: Transformation parameters
    """
    if allow_rotation:
        # Use full parameters for optimization (dx, dy, theta)
        initial_params = [0, 0, 0]
        result = least_squares(residuals, initial_params, args=(key_pairs,))
        dx, dy, theta = result.x
    else:
        # Only optimize translation parameters (dx, dy), theta fixed to 0
        initial_params = [0, 0]
        
        # Define new residual function using only translation parameters
        def residuals_no_rotation(params, key_pairs):
            dx, dy = params
            residual = []
            
            for original_key, observed_key, confidence in key_pairs:
                p_original = get_key_coordinates(original_key)
                p_observed = get_key_coordinates(observed_key)
                
                # Only perform translation transformation
                p_transformed = p_original + np.array([dx, dy])
                
                # Calculate residual with weighting
                res = (p_observed - p_transformed) * confidence
                residual.append(res)
            
            return np.concatenate(residual)
        
        result = least_squares(residuals_no_rotation, initial_params, args=(key_pairs,))
        dx, dy = result.x
        theta = 0.0
    
    return dx, dy, theta

def plot_keyboard_comparison(dx, dy, theta):
    """Plot comparison of original and transformed keyboards"""
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Key size (19mm edge length)
    key_size = 19
    
    # Create rotation matrix
    rotation_matrix = np.array([[np.cos(theta), -np.sin(theta)],
                              [np.sin(theta), np.cos(theta)]])
    
    # Draw original keyboard (blue)
    for key, (x, y) in keyboard_coordinates.items():
        # Original key (blue semi-transparent)
        rect = Rectangle((x - key_size/2, y - key_size/2), 
                        key_size, key_size, 
                        facecolor='none',
                        edgecolor='blue',
                        linewidth=1,
                        alpha=0.5)
        ax.add_patch(rect)
        ax.text(x, y, key, 
                color='blue',
                horizontalalignment='center',
                verticalalignment='center',
                fontsize=8,
                alpha=0.5)
    
    # Draw transformed keyboard (red)
    for key, (x, y) in keyboard_coordinates.items():
        # Calculate transformed center point
        original_point = np.array([x, y])
        transformed_point = rotation_matrix @ original_point + np.array([dx, dy])
        
        # Calculate four corner points after transformation
        corners = np.array([
            [-key_size/2, -key_size/2],
            [key_size/2, -key_size/2],
            [key_size/2, key_size/2],
            [-key_size/2, key_size/2]
        ])
        
        # Rotate corners and add offset
        transformed_corners = np.array([
            rotation_matrix @ corner + transformed_point
            for corner in corners
        ])
        
        # Draw transformed key using polygon
        plt.fill(transformed_corners[:, 0], transformed_corners[:, 1],
                facecolor='none', edgecolor='red', alpha=0.5)
        
        # Add transformed key text
        plt.text(transformed_point[0], transformed_point[1], key,
                color='red',
                horizontalalignment='center',
                verticalalignment='center',
                fontsize=8,
                alpha=0.5)
    
    # Set equal aspect ratio
    ax.set_aspect('equal')
    
    # Set axis range
    margin = 50  # Add margin
    ax.set_xlim(-20 - margin, 280 + margin)
    ax.set_ylim(-90 - margin, 20 + margin)
    
    # Add grid
    ax.grid(True, linestyle='--', alpha=0.3)
    
    # Add title and axis labels
    plt.title('Keyboard Layout Comparison (Blue: Original, Red: WLS Transformed)')
    plt.xlabel('X Coordinate (mm)')
    plt.ylabel('Y Coordinate (mm)')
    
    # Add legend
    blue_patch = plt.Rectangle((0, 0), 1, 1, fc='none', ec='blue', alpha=0.5, label='Original Layout')
    red_patch = plt.Rectangle((0, 0), 1, 1, fc='none', ec='red', alpha=0.5, label='Transformed Layout')
    ax.legend(handles=[blue_patch, red_patch])
    
    plt.show()

def read_predictions(file_path):
    """Read prediction results and convert to key_pairs format"""
    df = pd.read_csv(file_path)
    
    # Convert to key_pairs format: (true_key, predicted_key, probability)
    key_pairs = []
    for _, row in df.iterrows():
        key_pairs.append((
            row['true_key'],
            row['predicted_key'],
            row['probability']
        ))
    
    return key_pairs

def find_nearest_key(transformed_point, keyboard_coords):
    """Find the nearest keyboard key to a given point"""
    min_dist = float('inf')
    nearest_key = None
    
    for key, (x, y) in keyboard_coords.items():
        dist = np.linalg.norm(transformed_point - np.array([x, y]))
        if dist < min_dist:
            min_dist = dist
            nearest_key = key
    
    return nearest_key, min_dist

def analyze_string(input_string, dx, dy, theta):
    """Analyze the nearest original key for each character in the string on the transformed keyboard"""
    rotation_matrix = np.array([[np.cos(theta), -np.sin(theta)],
                              [np.sin(theta), np.cos(theta)]])
    
    print("\nString Analysis Results:")
    print("Input string:", input_string)
    print("-" * 50)
    print("Char | Transformed Position | Nearest Original Key | Distance(mm)")
    print("-" * 50)
    
    for char in input_string:
        if char.upper() in keyboard_coordinates:
            # Get character position on transformed keyboard
            original_point = np.array(keyboard_coordinates[char.upper()])
            transformed_point = rotation_matrix @ original_point + np.array([dx, dy])
            
            # Find nearest original key
            nearest_key, distance = find_nearest_key(transformed_point, keyboard_coordinates)
            
            print(f"{char:^4} | ({transformed_point[0]:.1f}, {transformed_point[1]:.1f}) | {nearest_key:^10} | {distance:.2f}")

def main():
    # Read prediction results from CSV file
    predictions_file = 'wooting_predictions_dy2cm.csv'
    key_pairs = read_predictions(predictions_file)
    
    # Print loaded key pair information
    print("Loaded key pairs:")
    for true_key, pred_key, prob in key_pairs:
        print(f"True: {true_key}, Predicted: {pred_key}, Probability: {prob:.4f}")

    # Estimate displacement and rotation (can choose whether to allow rotation)
    allow_rotation = False  # Can be set to False as needed
    dx, dy, theta = estimate_transform(key_pairs, allow_rotation)
    
    print(f"\nEstimated transformation parameters:")
    print(f"dx: {dx:.2f} mm")
    print(f"dy: {dy:.2f} mm")
    print(f"theta: {theta:.4f} radians ({np.degrees(theta):.2f} degrees)")

    # # Analyze specific string
    # input_string = "sudo rm -rf"
    # analyze_string(input_string, dx, dy, theta)

    # Draw comparison plot
    plot_keyboard_comparison(dx, dy, theta)

if __name__ == "__main__":
    main()

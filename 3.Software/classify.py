import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.metrics import confusion_matrix
import matplotlib.pyplot as plt

# ================================
# CONFIGURATION PARAMETERS
# ================================

# Data parameters
NUM_SENSORS = 8                    # Number of magnetic sensors in the array
NUM_AXES = 3                      # Number of axes per sensor (x, y, z)
INPUT_SIZE = NUM_SENSORS * NUM_AXES  # Total input features (24)

# Model architecture parameters
HIDDEN_SIZE = 64                  # Number of neurons in hidden layer
DROPOUT_RATE = 0.5               # Dropout rate for regularization

# Training parameters
BATCH_SIZE = 32                  # Batch size for training and validation
LEARNING_RATE = 0.001            # Learning rate for Adam optimizer
NUM_EPOCHS = 80                  # Number of training epochs
TEST_SIZE = 0.3                  # Proportion of data used for testing (70/30 split)
RANDOM_STATE = 42                # Random seed for reproducibility

# Visualization parameters
FONT_SIZE = 14                   # Base font size for plots
FIGURE_SIZE_ACCURACY = (10, 6)   # Figure size for accuracy curves
FIGURE_SIZE_CONFUSION = (15, 12) # Figure size for confusion matrix

# Data file
DATA_FILE = r'3.Software/wooting_peak_data2.csv'     # Input CSV file path
MODEL_FILE = r'3.Software/wooting_keypress_model2.pth'  # Output model file path

# ================================
# DATASET CLASS DEFINITION
# ================================

class KeypressDataset(Dataset):
    """
    Custom PyTorch Dataset class for keypress magnetic field data.
    
    This dataset handles the magnetic field readings from multiple sensors
    arranged in an array, where each sensor provides 3-axis measurements.
    """
    
    def __init__(self, X, y):
        """
        Initialize the dataset with features and labels.
        
        Args:
            X (numpy.ndarray): Feature matrix with shape (samples, sensors, axes)
            y (numpy.ndarray): Label array with shape (samples,)
        """
        self.X = torch.FloatTensor(X)
        self.y = torch.LongTensor(y)
    
    def __len__(self):
        """Return the total number of samples in the dataset."""
        return len(self.X)
    
    def __getitem__(self, idx):
        """
        Get a single sample from the dataset.
        
        Args:
            idx (int): Index of the sample to retrieve
            
        Returns:
            tuple: (features, label) pair
        """
        return self.X[idx], self.y[idx]

# ================================
# MODEL ARCHITECTURE DEFINITION
# ================================

class KeypressClassifier(nn.Module):
    """
    Neural network classifier for keypress detection using magnetic field data.
    
    This model processes magnetic field readings from a sensor array to classify
    which key was pressed on a keyboard. The architecture uses fully connected
    layers with dropout for regularization.
    """
    
    def __init__(self, num_classes):
        """
        Initialize the neural network architecture.
        
        Args:
            num_classes (int): Number of different keys to classify
        """
        super(KeypressClassifier, self).__init__()
        
        # Feature extraction layers
        # Transforms raw sensor data into higher-level features
        self.features = nn.Sequential(
            nn.Linear(INPUT_SIZE, HIDDEN_SIZE),  # Linear transformation
            nn.ReLU(),                           # Non-linear activation
            nn.Dropout(DROPOUT_RATE)             # Regularization to prevent overfitting
        )
        
        # Classification layer
        # Maps features to class probabilities
        self.classifier = nn.Sequential(
            nn.Linear(HIDDEN_SIZE, num_classes)
        )
    
    def forward(self, x):
        """
        Forward pass through the network.
        
        Args:
            x (torch.Tensor): Input tensor with shape (batch_size, 1, sensors, axes)
            
        Returns:
            torch.Tensor: Output logits with shape (batch_size, num_classes)
        """
        # Flatten input from (batch_size, 1, 8, 3) to (batch_size, 24)
        x = x.view(x.size(0), -1)
        
        # Extract features
        x = self.features(x)
        
        # Classify
        x = self.classifier(x)
        
        return x

# ================================
# VISUALIZATION FUNCTIONS
# ================================

def plot_accuracy_curves(train_accuracies, val_accuracies):
    """
    Plot training and validation accuracy curves over epochs.
    
    This function creates a line plot showing how the model's performance
    improves during training on both training and validation sets.
    
    Args:
        train_accuracies (list): List of training accuracies for each epoch
        val_accuracies (list): List of validation accuracies for each epoch
    """
    plt.figure(figsize=FIGURE_SIZE_ACCURACY)
    epochs = range(1, len(train_accuracies) + 1)
    
    # Plot training and validation accuracy curves
    plt.plot(epochs, train_accuracies, 'b-', label='Training Accuracy', linewidth=2)
    plt.plot(epochs, val_accuracies, 'r-', label='Validation Accuracy', linewidth=2)
    
    # Configure plot appearance
    plt.title('Training and Validation Accuracy Over Time', fontsize=FONT_SIZE + 4)
    plt.xlabel('Epoch', fontsize=FONT_SIZE + 2)
    plt.ylabel('Accuracy (%)', fontsize=FONT_SIZE + 2)
    plt.legend(fontsize=FONT_SIZE)
    plt.grid(True, alpha=0.3)
    
    # Improve layout and display
    plt.tight_layout()
    plt.show()

def plot_confusion_matrix(y_true, y_pred, labels):
    """
    Plot confusion matrix to visualize classification performance.
    
    The confusion matrix shows how often each true class is predicted as
    each possible class, helping identify which keys are most confused.
    
    Args:
        y_true (list): True labels for test samples
        y_pred (list): Predicted labels for test samples  
        labels (list): List of class names for labeling
    """
    # Set consistent font size for all plot elements
    plt.rcParams.update({'font.size': FONT_SIZE})
    
    # Create display labels, replacing 'WHY' with '/' for better readability
    display_labels = [label if label != 'WHY' else '/' for label in labels]
    
    # Compute confusion matrix
    cm = confusion_matrix(y_true, y_pred, labels=range(len(labels)))
    
    # Create the figure
    plt.figure(figsize=FIGURE_SIZE_CONFUSION)
    
    # Plot heatmap using seaborn for better aesthetics
    sns.heatmap(cm, 
                annot=True,                      # Show numerical values
                fmt='d',                         # Integer format
                cmap='Blues',                    # Blue color scheme
                square=True,                     # Keep cells square
                xticklabels=display_labels,      # X-axis labels
                yticklabels=display_labels,      # Y-axis labels
                annot_kws={'size': FONT_SIZE},   # Font size for matrix values
                cbar_kws={'label': 'Number of Samples'}  # Colorbar label
                )
    
    # Add plot labels and title
    plt.title('Confusion Matrix - Keypress Classification Results', 
              fontsize=FONT_SIZE + 4, pad=20)
    plt.xlabel('Predicted Key', fontsize=FONT_SIZE + 2)
    plt.ylabel('True Key', fontsize=FONT_SIZE + 2)
    
    # Configure tick labels
    plt.xticks(rotation=45, ha='right', fontsize=FONT_SIZE)
    plt.yticks(rotation=0, fontsize=FONT_SIZE)
    
    # Adjust layout to ensure all labels are visible
    plt.tight_layout()
    plt.show()

# ================================
# MAIN TRAINING AND EVALUATION
# ================================

def main():
    """
    Main function that orchestrates the entire machine learning pipeline.
    
    This function handles:
    1. Data loading and preprocessing
    2. Model initialization and training
    3. Performance evaluation
    4. Visualization of results
    5. Model saving
    """
    
    # ================================
    # DATA LOADING AND PREPROCESSING
    # ================================
    
    print("Loading data from CSV file...")
    data = pd.read_csv(DATA_FILE)
    
    # Check for missing values in the dataset
    print("Checking for missing values in the data:")
    missing_values = data.isnull().sum()
    print(missing_values)
    
    # Remove rows containing missing values if any exist
    if missing_values.sum() > 0:
        print(f"Removing {missing_values.sum()} rows with missing values...")
        data = data.dropna()
    
    # Optional: Exclude numeric keys (0-9) from training
    # This can be uncommented if you want to focus on letter keys only
    # exclude_keys = [str(i) for i in range(10)]
    # data = data[~data.iloc[:, -1].isin(exclude_keys)]
    # print(f"Excluded numeric keys: {exclude_keys}")
    
    # Display unique class labels in the dataset
    unique_labels = data.iloc[:, -1].unique()
    print(f"\nUnique key labels found in dataset: {sorted(unique_labels)}")
    print(f"Total number of classes: {len(unique_labels)}")
    
    # Separate features and labels
    X = data.iloc[:, :-1].values  # All columns except the last (features)
    y = data.iloc[:, -1].values   # Last column (labels)
    
    print(f"Dataset shape: {X.shape}")
    print(f"Labels shape: {y.shape}")
    
    # ================================
    # LABEL ENCODING
    # ================================
    
    # Encode string labels to integers for neural network training
    print("Encoding labels...")
    le = LabelEncoder()
    y_encoded = le.fit_transform(y)
    
    print(f"Label mapping:")
    for i, label in enumerate(le.classes_):
        print(f"  {label} -> {i}")
    
    # ================================
    # DATA RESHAPING
    # ================================
    
    # Reshape features to match expected input format:
    # (samples, channels, sensors, axes) = (N, 1, 8, 3)
    print("Reshaping feature data...")
    X_reshaped = X.reshape(-1, 1, NUM_SENSORS, NUM_AXES)
    print(f"Reshaped data: {X_reshaped.shape}")
    
    # ================================
    # TRAIN-TEST SPLIT
    # ================================
    
    print(f"Splitting data into train ({int((1-TEST_SIZE)*100)}%) and test ({int(TEST_SIZE*100)}%) sets...")
    X_train, X_test, y_train, y_test = train_test_split(
        X_reshaped, y_encoded, 
        test_size=TEST_SIZE, 
        random_state=RANDOM_STATE, 
        stratify=y_encoded  # Ensure balanced split across classes
    )
    
    print(f"Training set: {X_train.shape[0]} samples")
    print(f"Test set: {X_test.shape[0]} samples")
    
    # ================================
    # DATASET AND DATALOADER CREATION
    # ================================
    
    print("Creating datasets and data loaders...")
    train_dataset = KeypressDataset(X_train, y_train)
    test_dataset = KeypressDataset(X_test, y_test)
    
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=BATCH_SIZE, shuffle=False)
    
    print(f"Training batches: {len(train_loader)}")
    print(f"Test batches: {len(test_loader)}")
    
    # ================================
    # MODEL INITIALIZATION
    # ================================
    
    # Check for GPU availability
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Using device: {device}")
    
    # Initialize model
    num_classes = len(le.classes_)
    model = KeypressClassifier(num_classes).to(device)
    
    print(f"Model initialized with {num_classes} output classes")
    print(f"Model parameters: {sum(p.numel() for p in model.parameters())} total")
    
    # ================================
    # LOSS FUNCTION AND OPTIMIZER
    # ================================
    
    # Cross-entropy loss for multi-class classification
    criterion = nn.CrossEntropyLoss()
    
    # Adam optimizer with specified learning rate
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)
    
    print(f"Using CrossEntropyLoss and Adam optimizer (lr={LEARNING_RATE})")
    
    # ================================
    # TRAINING LOOP
    # ================================
    
    print(f"\nStarting training for {NUM_EPOCHS} epochs...")
    
    # Lists to store accuracy history for plotting
    train_accuracies = []
    val_accuracies = []
    
    for epoch in range(NUM_EPOCHS):
        # ----------------
        # TRAINING PHASE
        # ----------------
        model.train()  # Set model to training mode
        train_correct = 0
        train_total = 0
        total_loss = 0
        
        for batch_idx, (inputs, labels) in enumerate(train_loader):
            # Move data to device (GPU/CPU)
            inputs, labels = inputs.to(device), labels.to(device)
            
            # Reset gradients
            optimizer.zero_grad()
            
            # Forward pass
            outputs = model(inputs)
            loss = criterion(outputs, labels)
            
            # Backward pass and optimization
            loss.backward()
            optimizer.step()
            
            # Calculate statistics
            total_loss += loss.item()
            _, predicted = torch.max(outputs.data, 1)
            train_total += labels.size(0)
            train_correct += (predicted == labels).sum().item()
        
        # Calculate training accuracy
        train_accuracy = 100 * train_correct / train_total
        train_accuracies.append(train_accuracy)
        
        # ----------------
        # VALIDATION PHASE
        # ----------------
        model.eval()  # Set model to evaluation mode
        val_correct = 0
        val_total = 0
        
        with torch.no_grad():  # Disable gradient computation for efficiency
            for inputs, labels in test_loader:
                inputs, labels = inputs.to(device), labels.to(device)
                outputs = model(inputs)
                _, predicted = torch.max(outputs.data, 1)
                val_total += labels.size(0)
                val_correct += (predicted == labels).sum().item()
        
        # Calculate validation accuracy
        val_accuracy = 100 * val_correct / val_total
        val_accuracies.append(val_accuracy)
        
        # Print epoch results
        print(f'Epoch [{epoch+1}/{NUM_EPOCHS}]:')
        print(f'  Average Loss: {total_loss/len(train_loader):.4f}')
        print(f'  Training Accuracy: {train_accuracy:.2f}%')
        print(f'  Validation Accuracy: {val_accuracy:.2f}%')
    
    # ================================
    # FINAL EVALUATION
    # ================================
    
    print(f'\n{"="*50}')
    print(f'FINAL RESULTS')
    print(f'{"="*50}')
    print(f'Final Test Accuracy: {val_accuracy:.2f}%')
    
    # ================================
    # VISUALIZATION
    # ================================
    
    print("Generating accuracy curves...")
    plot_accuracy_curves(train_accuracies, val_accuracies)
    
    # ================================
    # CONFUSION MATRIX CALCULATION
    # ================================
    
    print("Computing confusion matrix...")
    model.eval()
    all_predictions = []
    all_true_labels = []
    
    with torch.no_grad():
        for inputs, labels in test_loader:
            inputs, labels = inputs.to(device), labels.to(device)
            outputs = model(inputs)
            _, predicted = torch.max(outputs.data, 1)
            
            # Collect predictions and true labels
            all_predictions.extend(predicted.cpu().numpy())
            all_true_labels.extend(labels.cpu().numpy())
    
    # Convert encoded labels back to original string labels
    label_names = le.inverse_transform(np.unique(all_true_labels))
    
    # Plot confusion matrix
    print("Generating confusion matrix...")
    plot_confusion_matrix(all_true_labels, all_predictions, label_names)
    
    # ================================
    # MODEL SAVING
    # ================================
    
    print(f"Saving model to {MODEL_FILE}...")
    # Move model to CPU before saving for better compatibility
    model_cpu = model.cpu()
    torch.save({
        'model_state_dict': model_cpu.state_dict(),
        'label_encoder': le,
        'num_classes': num_classes,
        'model_config': {
            'input_size': INPUT_SIZE,
            'hidden_size': HIDDEN_SIZE,
            'dropout_rate': DROPOUT_RATE,
            'num_sensors': NUM_SENSORS,
            'num_axes': NUM_AXES
        }
    }, MODEL_FILE)
    
    print("Training completed successfully!")

# ================================
# SCRIPT ENTRY POINT
# ================================

if __name__ == "__main__":
    main()

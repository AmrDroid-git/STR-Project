import time
import joblib
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.linear_model import LogisticRegression
from sklearn.tree import DecisionTreeClassifier
from sklearn.metrics import accuracy_score, f1_score, confusion_matrix

# 1. Load dataset
df = pd.read_csv("data_correspondances.csv")
FEATURES = [
    "retard_min",
    "nb_correspondances",
    "temps_restant_min",
    "deplacement_min",
    "securite_min",
]
X = df[FEATURES].to_numpy()
y = df["manquee"].to_numpy()

# 2. Stratified train/test split (80% train / 20% test)
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)

# 3. Train & compare lightweight real-time models
models = {
    "Logistic Regression": LogisticRegression(max_iter=1000),
    "Decision Tree (depth=4)": DecisionTreeClassifier(max_depth=4, random_state=42),
}

best_model = None
best_acc = 0.0
best_name = ""

for name, model in models.items():
    model.fit(X_train, y_train)
    y_pred = model.predict(X_test)
    acc = accuracy_score(y_test, y_pred)
    f1 = f1_score(y_test, y_pred)
    cm = confusion_matrix(y_test, y_pred)

    # Measure single-prediction latency (WCET estimation)
    sample = X_test[0:1]
    model.predict(sample)  # warm-up call
    times_ms = []
    for _ in range(1000):
        t0 = time.perf_counter()
        model.predict(sample)
        times_ms.append((time.perf_counter() - t0) * 1000)

    print(f"=== {name} ===")
    print(f"Accuracy : {acc * 100:.2f}% | F1-score: {f1:.4f}")
    print(f"Confusion Matrix:\n{cm}")
    print(f"Latency  : Avg = {np.mean(times_ms):.4f} ms | Max (WCET) = {np.max(times_ms):.4f} ms\n")

    if acc > best_acc:
        best_acc = acc
        best_model = model
        best_name = name

# 4. Save the best model
joblib.dump(best_model, "modele_correspondance.joblib")
print(f"Saved best model ({best_name}, {best_acc * 100:.2f}%) -> modele_correspondance.joblib")

# 5. Export weights if Logistic Regression won (useful for direct C/embedded use)
if isinstance(best_model, LogisticRegression):
    print("\n--- Logistic Regression Parameters ---")
    print(f"Intercept : {best_model.intercept_[0]:.4f}")
    for feat, coef in zip(FEATURES, best_model.coef_[0]):
        print(f"  {feat:20s}: {coef:.4f}")
import joblib
import numpy as np

# Load once at system startup (outside the real-time loop)
model = joblib.load("modele_correspondance.joblib")

def predict_connection_risk(retard, nb_corr, temps_restant, deplacement, securite):
    """Returns (decision_label, probability_missed)."""
    x = np.array([[retard, nb_corr, temps_restant, deplacement, securite]])
    prob_missed = model.predict_proba(x)[0][1]

    if prob_missed >= 0.70:
        action = "URGENT: Delay connecting flight / Deploy priority shuttle"
    elif prob_missed >= 0.40:
        action = "WARNING: Open Fast-Track security & alert gate"
    else:
        action = "NORMAL: Standard transfer"

    return action, prob_missed

# Example test: Flight delayed 25 min, 28 passengers, 20 min left, 14 min walk, 10 min security
action, prob = predict_connection_risk(25, 28, 20, 14, 10)
print(f"Risk of missed connection: {prob * 100:.1f}% -> {action}")

tests = [
    ("Avion A (Normal)",  5, 10, 50,  8,  5),
    ("Avion B (Warning)", 15, 20, 34, 12,  8),
    ("Avion C (Urgent)",  25, 28, 20, 14, 10),
]

for name, r, nb, tr, dep, sec in tests:
    action, prob = predict_connection_risk(r, nb, tr, dep, sec)
    print(f"{name:18s} -> Risk: {prob*100:5.1f}% | {action}")
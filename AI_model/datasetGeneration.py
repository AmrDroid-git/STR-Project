import numpy as np
import pandas as pd

rng = np.random.default_rng(42)
n = 3000

retard_min = rng.integers(0, 46, n)                    # 0..45
nb_correspondances = rng.integers(5, 41, n)            # 5..40
planned_transfer_time = rng.integers(30, 91, n)        # 30..90 (intermediate)
temps_restant_min = np.maximum(planned_transfer_time - retard_min, 0)
deplacement_min = rng.integers(3, 21, n)               # 3..20
securite_min = rng.integers(2, 16, n)                  # 2..15

time_needed = (
    5
    + deplacement_min
    + securite_min
    + 0.5 * nb_correspondances
    + rng.normal(0, 4, n)
)
manquee = (time_needed > temps_restant_min).astype(int)

df = pd.DataFrame({
    "retard_min": retard_min,
    "nb_correspondances": nb_correspondances,
    "temps_restant_min": temps_restant_min,
    "deplacement_min": deplacement_min,
    "securite_min": securite_min,
    "manquee": manquee,
})

df.to_csv("data_correspondances.csv", index=False)

print("Shape:", df.shape)
print("\nClass balance (%):")
print((df["manquee"].value_counts(normalize=True) * 100).round(2))
print("\nMissing values:", int(df.isna().sum().sum()))
print("\nFirst 5 rows:")
print(df.head())
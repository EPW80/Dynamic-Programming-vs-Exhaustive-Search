////////////////////////////////////////////////////////////////////////////////
// maxweight.hh
//
// Compute the set of foods that maximizes the weight in foods, within 
// a given maximum calorie amount with the dynamic programming or exhaustive search.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <vector>


// One food item available for purchase.
class FoodItem {
  //
  public:

    //
    FoodItem(
      const std::string & description,
        double calories,
        double weight_ounces
    ): _description(description),
  _calories(calories),
  _weight_ounces(weight_ounces) {
    assert(!description.empty());
    assert(calories > 0);
  }

  //
  const std::string & description() const {
    return _description;
  }
  double calorie() const {
    return _calories;
  }
  double weight() const {
    return _weight_ounces;
  }

  //
  private:

    // Human-readable description of the food, e.g. "spicy chicken breast". Must be non-empty.
    std::string _description;

  // Calories; Must be positive
  double _calories;

  // Food weight, in ounces; most be non-negative.
  double _weight_ounces;
};

// Alias for a vector of shared pointers to FoodItem objects.
typedef std::vector < std::shared_ptr < FoodItem >> FoodVector;

// Load all the valid food items from the CSV database
// Food items that are missing fields, or have invalid values, are skipped.
// Returns nullptr on I/O error.
std::unique_ptr < FoodVector > load_food_database(const std::string & path) {
  std::unique_ptr < FoodVector > failure(nullptr);

  std::ifstream f(path);
  if (!f) {
    std::cout << "Failed to load food database; Cannot open file: " << path << std::endl;
    return failure;
  }

  std::unique_ptr < FoodVector > result(new FoodVector);

  size_t line_number = 0;
  for (std::string line; std::getline(f, line);) {
    line_number++;

    // First line is a header row
    if (line_number == 1) {
      continue;
    }

    std::vector < std::string > fields;
    std::stringstream ss(line);

    for (std::string field; std::getline(ss, field, '^');) {
      fields.push_back(field);
    }

    if (fields.size() != 3) {
      std::cout <<
        "Failed to load food database: Invalid field count at line " << line_number << "; Want 3 but got " << fields.size() << std::endl <<
        "Line: " << line << std::endl;
      return failure;
    }

    std::string
    descr_field = fields[0],
      calories_field = fields[1],
      weight_ounces_field = fields[2];

    auto parse_dbl = [](const std::string & field, double & output) {
      std::stringstream ss(field);
      if (!ss) {
        return false;
      }

      ss >> output;

      return true;
    };

    std::string description(descr_field);
    double calories, weight_ounces;
    if (
      parse_dbl(calories_field, calories) &&
      parse_dbl(weight_ounces_field, weight_ounces)
    ) {
      result -> push_back(
        std::shared_ptr < FoodItem > (
          new FoodItem(
            description,
            calories,
            weight_ounces
          )
        )
      );
    }
  }

  f.close();

  return result;
}

// Convenience function to compute the total weight and calories in 
// a FoodVector.
// Provide the FoodVector as the first argument
// The next two arguments will return the weight and calories back to 
// the caller.
void sum_food_vector
  (
    const FoodVector & foods,
      double & total_calories,
      double & total_weight
  ) {
    total_calories = total_weight = 0;
    for (auto & food: foods) {
      total_calories += food -> calorie();
      total_weight += food -> weight();
    }
  }

// Convenience function to print out each FoodItem in a FoodVector,
// followed by the total weight and calories of it.
void print_food_vector(const FoodVector & foods) {
  std::cout << "*** food Vector ***" << std::endl;

  if (foods.size() == 0) {
    std::cout << "[empty food list]" << std::endl;
  } else {
    for (auto & food: foods) {
      std::cout <<
        "Ye olde " << food -> description() <<
        " ==> " <<
        "; calories = " << food -> calorie() <<
        "Weight of " << food -> weight() << " ounces" <<
        std::endl;
    }

    double total_calories, total_weight;
    sum_food_vector(foods, total_calories, total_weight);
    std::cout <<
      "> Grand total calories: " << total_calories <<
      std::endl <<
      "> Grand total weight: " << total_weight << " ounces" << std::endl;
  }
}

// Filter the vector source, i.e. create and return a new FoodVector
// containing the subset of the food items in source that match given
// criteria.
// This is intended to:
//	1) filter out food with zero or negative weight that are irrelevant to // our optimization
//	2) limit the size of inputs to the exhaustive search algorithm since it // will probably be slow.
//
// Each food item that is included must have at minimum min_weight and 
// at most max_weight.
//	(i.e., each included food item's weight must be between min_weight
// and max_weight (inclusive).
//
// In addition, the vector includes only the first total_size food items
// that match these criteria.
std::unique_ptr < FoodVector > filter_food_vector(
  const FoodVector & source, // Source vector containing food items to be filtered.
    double min_weight, // Minimum weight threshold for food items to be included.
    double max_weight, // Maximum weight threshold for food items to be included.
    int total_size) { // Maximum number of food items to include in the result.

  // Create an empty vector to store the filtered results.
  auto result = std::make_unique < FoodVector > ();

  // Iterate through each item in the source vector.
  for (const auto & item: source) {

    // Check if the item's weight is within the specified range.
    if (item -> weight() >= min_weight && item -> weight() <= max_weight) {

      // If it is, add the item to the result vector.
      result -> push_back(item);

      // If the result vector has reached the specified maximum size, break out of the loop.
      if (result -> size() == static_cast < size_t > (total_size)) break;
    }
  }
  return result;
}

// Compute the optimal set of food items with dynamic programming.
// Specifically, among the food items that fit within a total_calories,
// choose the foods whose weight-per-calorie is largest.
// Repeat until no more food items can be chosen, either because we've 
// run out of food items, or run out of space.
std::unique_ptr<FoodVector> dynamic_max_weight(const FoodVector& foods, double total_calories_input) {
    // Convert total_calories to integer
    int total_calories = static_cast<int>(total_calories_input);
    std::vector<std::vector<double>> dp(foods.size() + 1, std::vector<double>(total_calories + 1, 0));
    std::vector<std::vector<bool>> take(foods.size() + 1, std::vector<bool>(total_calories + 1, false));

    // Fill the DP table
    for (size_t i = 1; i <= foods.size(); ++i) {
        for (int w = 0; w <= total_calories; ++w) {
            if (foods[i-1]->calorie() > w) {
                dp[i][w] = dp[i-1][w];
            } else {
                double weight_if_taken = dp[i-1][w - foods[i-1]->calorie()] + foods[i-1]->weight();
                if (weight_if_taken > dp[i-1][w]) {
                    dp[i][w] = weight_if_taken;
                    take[i][w] = true;
                } else {
                    dp[i][w] = dp[i-1][w];
                }
            }
        }
    }

    // Reconstruct the optimal set of food items
    std::unique_ptr<FoodVector> result(new FoodVector);
    int calorie_remaining = total_calories;
    for (int i = foods.size(); i > 0; --i) {
        if (take[i][calorie_remaining]) {
            // Take the food item
            result->insert(result->begin(), foods[i-1]);
            calorie_remaining -= foods[i-1]->calorie();
        }
    }

    return result;
}



// Compute the optimal set of food items with a exhaustive search algorithm.
// Specifically, among all subsets of food items, return the subset 
// whose weight in ounces fits within the total_weight one can carry and
// whose total calories is greatest.
// To avoid overflow, the size of the food items vector must be less than 64.
std::unique_ptr < FoodVector > exhaustive_max_weight(const FoodVector & foods, double total_calorie) {
  auto best_subset = std::make_unique < FoodVector > ();
  double best_weight = 0.0;

  // Calculate the total number of possible subsets by shifting 1 left by the number of food items.
  size_t subsetCount = 1ULL << foods.size();
  for (size_t i = 0; i < subsetCount; ++i) {
    // Create a new empty subset
    auto current_subset = std::make_unique < FoodVector > ();
    double current_weight = 0.0, current_calories = 0.0;

    for (size_t j = 0; j < foods.size(); ++j) {
      // Check if the jth bit is set in the ith subset
      if (i & (1ULL << j)) {
        // Add the jth food item to the current subset
        current_subset -> push_back(foods[j]);
        current_weight += foods[j] -> weight();
        current_calories += foods[j] -> calorie();
      }
    }

    if (current_calories <= total_calorie && current_weight > best_weight) {
      // Update the best weight and best subset found.
      best_weight = current_weight;
      best_subset = std::move(current_subset);
    }
  }
  return best_subset;
}
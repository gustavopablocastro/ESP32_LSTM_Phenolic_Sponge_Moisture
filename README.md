# ESP32 LSTM Phenolic Sponge Moisture

## Introduction
This repository has files obtained from the development of the final work carried out during the Postgraduate Course in Robotics and Artificial Intelligence (PRIA) taught by the Universidad Tecnológica de Uruguay - UTEC (URU) and the Universidade Federal do Rio Grande - FURG (BRA).

Among its objectives were:
- obtain a dataset of covariates that influence the humidity behavior of germinating phenolic sponges in hydroponic agronomy.
- the generation of an RNN - LSTM network model for the prediction of future humidity of the germination sponge
- the deployment to an embedded system that alerts about the future prediction of values ​​outside those recommended for germination

## From the repository
In this repository you will find
- the circuit and firmware of the built datalogger (ARDUINO Framework)
- the dataset surveyed in autumn, in the city of Rafaela, Santa Fe, Argentina, which is intended to be completed in the four seasons of the year.
- the notebook with the dataset preprocessing code, model generation and training, and the deployment to the ARDUINO library using the EdgeImpulse python SDK
- the circuit and firmware of the developed embedded system

The figure below shows the accuracy in operation, predicting 30 minutes (3 steps) into the future.
![](https://github.com/gustavopablocastro/ESP32_LSTM_Phenolic_Sponge_Moisture/blob/main/Embedded_System/Pictures/Sistema_embebido_ensayo.png)

Where the values ​​of the measured variables are shown according to the following references:
 - item a = Room Temperature
 - item b = Ambient Relative Humidity
 - item c = Atmospheric Pressure
 - item d = Relative Humidity Germination foam
 - item e = Predicted Relative Humidity for Germination Foam, 30 minutes in the future.

-------------------------------------------------------------------------
Ingeniero Gustavo Castro - Julio 2024
